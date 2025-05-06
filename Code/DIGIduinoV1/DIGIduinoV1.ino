
/*
  The Printable Watch 2025
  theprintablewatch.com
*/

#include "SevSeg.h"
//#include "LowPower.h"
#include "RtcDS1302.h"

// ------------------ RTC Setup ------------------
ThreeWire myWire(14, 15, 16); // IO (DATA), SCLK (CLK), CE (RST)
RtcDS1302<ThreeWire> Rtc(myWire);

// ------------------ SevSeg Setup ------------------
SevSeg sevseg; 

// ------------------ Pins ------------------
const byte BUTTON_WAKE_PIN    = 2;   // Wake-up button (external interrupt)
const byte BUTTON_HOUR_PIN    = 19;  // Hour increment button
const byte BUTTON_MINUTE_PIN  = 18;  // Minute increment button
const byte BUTTON_SPARE_PIN   = 0;   // Enter time-set mode (held 2 seconds)

// ------------------ Interrupt Flag ------------------
volatile bool wakeInterruptTriggered = false;

// ------------------ Times & Delays ------------------
unsigned long currentMillis      = 0;
unsigned long lastInteraction    = 0;       // Tracks when a button was last pressed
const unsigned long wakeInterval = 8000;    // Display active for 5s if no further interaction
const unsigned long holdDuration = 2000;    // 2s hold to enter time-set mode
const unsigned long showSetTime  = 2000;    // 2s to display "SET" before entering time-set

// ------------------ State Machine ------------------
enum WatchState {
  NORMAL,
  SHOWDATE,
  SHOWSET,
  TIMESET,
  SHOWDSET,
  DATESET,
  SLEEPING
};

// ------------------- Moon Phases ------------------
const byte moonPhases[8][4] = {

  {0x00, 0x00, 0x00, 0x00},       // 0: New Moon
  {0x00, 0x00, 0x00, 0x0F},       // 1: Waxing Crescent
  {0x00, 0x00, 0x39, 0x0F},       // 2: First Quarter
  {0x00, 0x39, 0x09, 0x0F},       // 3: Waxing Gibbous
  {0x39, 0x09, 0x09, 0x0F},       // 4: Full Moon
  {0x39, 0x09, 0x0F, 0x00},       // 5: Waning Gibbous (fixed your typo)
  {0x39, 0x0F, 0x00, 0x00},       // 6: Last Quarter
  {0x39, 0x00, 0x00, 0x00}        // 7: Waning Crescent

};

byte getMoonPhase(int year, byte month, byte day) {
  // Adjust months for algorithm (Jan & Feb are 13 & 14 of previous year)
  if (month < 3) {
    year--;
    month += 12;
  }

  // Calculate Julian Day Number (JDN)
  long a = year / 100;
  long b = 2 - a + a / 4;
  long jd = (long)(365.25 * (year + 4716)) + (int)(30.6001 * (month + 1)) + day + b - 1524.5;

  // Days since known new moon on 2000 Jan 6 at 18:14 UTC (JD = 2451550.1)
  float daysSinceNew = jd - 2451550.1;

  // Moon age in days (modulo synodic month)
  float synodicMonth = 29.53058867;
  float age = fmod(daysSinceNew, synodicMonth);
  if (age < 0) age += synodicMonth;

  // Convert age to phase index 0–7
  byte index = (byte)((age / synodicMonth) * 8 + 0.5);  // round to nearest
  return index & 7;  // wrap to 0–7
}

WatchState watchState = NORMAL;

// ------------------ Spare Pin Holding Logic ------------------
bool sparePinHeld        = false;
bool sparePinHolding     = false;
unsigned long sparePinPressTime = 0;

// ------------------ Hour Pin Holding Logic  (used for setting date)-----
bool hourPinHeld        = false;
bool hourPinHolding     = false;
unsigned long hourPinPressTime = 0;

// ------------------ Hour/Minute Buttons ------------------
int hourPbState       = LOW;
int lastHourPbState   = LOW;
int minPbState        = LOW;
int lastMinPbState    = LOW;

// ------------------ Time Setting Variables ------------------
int setHour   = 0;
int setMinute = 0;
int setDay   = 0;
int setMonth = 0;
// ------------------ Global Time Variables -------------------
RtcDateTime now;
int hour = 0;
int minute = 0;
int timeCombined = 8888;
int day = 1;
int month = 1;
int dateCombined = 101;

void setup() 
{
  // ------------------ Pin Modes ------------------
  pinMode(BUTTON_WAKE_PIN, INPUT);
  pinMode(BUTTON_HOUR_PIN, INPUT);
  pinMode(BUTTON_MINUTE_PIN, INPUT);
  pinMode(BUTTON_SPARE_PIN, INPUT);

  // Attach interrupt for wake button
  attachInterrupt(digitalPinToInterrupt(BUTTON_WAKE_PIN), isrWake, FALLING);

  // ------------------ Seven-Segment Setup ------------------
  byte numDigits = 4;
  byte digitPins[] = {1, 3, 4, 5};
  byte segmentPins[] = {6, 7, 8, 9, 10, 11, 12, 13};
  bool resistorsOnSegments = false;           // 'false' → resistors on digit pins
  byte hardwareConfig = COMMON_CATHODE;       // On your PCB
  bool updateWithDelays = false;             // Recommended = false
  bool leadingZeros = true;                  // Display leading zeros
  bool disableDecPoint = true;               // No decimal point used

  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, 
               resistorsOnSegments, updateWithDelays, leadingZeros, 
               disableDecPoint);

  // Set a default brightness
  sevseg.setBrightness(60);

  // ------------------ RTC Setup ------------------
  Rtc.Begin();
  // Set the RTC to compile time ONCE at startup (remove if not desired):
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Rtc.SetDateTime(compiled);


  // ------------------ RTC Setup ------------------
  // Disable any unused pins

  pinMode(17, OUTPUT);
  pinMode(20, OUTPUT);
  pinMode(21, OUTPUT);
  pinMode(22, OUTPUT);



  // Optional: briefly display "beta" (or "TPW") on startup
  unsigned long start = millis();
  while(millis() - start < 2000) {
    sevseg.setChars("hey");
    sevseg.refreshDisplay();
  }

  // Initialise tracking of last interaction to now
  lastInteraction = millis();
}

void loop()
{
  currentMillis = millis();

  // Refresh the display often
  sevseg.refreshDisplay();

  // State machine
  switch(watchState)
  {
    case NORMAL:
      handleNormalMode();
      break;
    
    case SHOWDATE:
    handleShowDateMode();
    break;

    case SHOWSET:
      handleShowSetMode();
      break;

    case TIMESET:
      handleTimeSetMode();
      break;

    case SHOWDSET:
      handleShowDSetMode();
      break;

    case DATESET:
      handleDateSetMode();
      break;

    case SLEEPING:
      // Code only returns here after interrupt sets watchState to NORMAL.
      // So there's nothing special to do in the loop if watchState == SLEEPING.
      break;
  }
}

// ----------------------------------------------------------
//                  NORMAL MODE
// ----------------------------------------------------------
void handleNormalMode()
{
  // If interrupt triggered, we've just woken up → reset things
  if(wakeInterruptTriggered) {
    wakeInterruptTriggered = false;
    lastInteraction = millis();
  }

//check if min button is pressed, display date if it is
  if (digitalRead(BUTTON_MINUTE_PIN) == HIGH) {
    watchState = SHOWDATE;
    return;
  }
  //Time is read once globally before displaying time, reduces flickering
  sevseg.setNumber(timeCombined, 1);

  // Check for inactivity → sleep
  if(currentMillis - lastInteraction > wakeInterval) {
    goToSleep();
    return;
  }

  // Handle Spare Pin (enter time-set if held 2s)
  checkSparePinHold();
  checkHourPinHold();

}
// ----------------------------------------------------------
//                  DATE MODE - With moonphse
// ----------------------------------------------------------
void handleShowDateMode() {
  static unsigned long lastToggleTime = 0;
  static bool showingDate = true;

  if (digitalRead(BUTTON_MINUTE_PIN) == HIGH) {
    unsigned long currentMillis = millis();

    // Toggle every 1000ms (1 second)
    if (currentMillis - lastToggleTime >= 1000) {
      showingDate = !showingDate;
      lastToggleTime = currentMillis;
    }

    if (showingDate) {
      RtcDateTime now = Rtc.GetDateTime();
      int dateCombined = (now.Day() * 100) + now.Month();
      sevseg.setNumber(dateCombined, 1); // 1 = leading zeros
    } else {
      // Get moon phase
      RtcDateTime now = Rtc.GetDateTime();
      byte moonPhase = getMoonPhase(now.Year(), now.Month(), now.Day());
      sevseg.setSegments(moonPhases[moonPhase]);
    }

    sevseg.refreshDisplay(); // Update display

  } else {
    // Button released → return to NORMAL state
    watchState = NORMAL;
    lastInteraction = millis();
  }
}

// ----------------------------------------------------------
//                   SHOWSET MODE
//   Display "SET" for 2 seconds, then go into TIMESET
// ----------------------------------------------------------
void handleShowSetMode()
{
  // Simply display "SET" for showSetTime (2s)
  sevseg.setChars("SET ");

  if(currentMillis - sparePinPressTime >= showSetTime) {
    // After 2 seconds, enter TIMESET
    // Grab current RTC time as a starting point
    RtcDateTime now = Rtc.GetDateTime();
    setHour   = now.Hour();
    setMinute = now.Minute();

    watchState = TIMESET;
    lastInteraction = millis(); // reset inactivity timer
  }
}

// ----------------------------------------------------------
//                   TIMESET MODE
//   Press hour/minute buttons to set time. If no press for
//   5 seconds, lock in and sleep.
// ----------------------------------------------------------
void handleTimeSetMode()
{
  // Display the 'setHour' and 'setMinute' being edited
  int timeCombined = (setHour * 100) + setMinute;
  sevseg.setNumber(timeCombined, 1);

  // If no interaction for 5s, commit the new time and sleep
  if(currentMillis - lastInteraction > wakeInterval) {
    RtcDateTime now = Rtc.GetDateTime();
    // Lock in the new time (day/month/year remain the same)
    Rtc.SetDateTime(RtcDateTime(now.Year(), now.Month(), now.Day(), 
                                setHour, setMinute, now.Second()));
    goToSleep();
    return;
  }

  // Check hour button
  hourPbState = digitalRead(BUTTON_HOUR_PIN);
  if(hourPbState == LOW && lastHourPbState == HIGH) {
    setHour = (setHour + 1) % 24;
    lastInteraction = millis(); // user pressed something
  }
  lastHourPbState = hourPbState;

  // Check minute button
  minPbState = digitalRead(BUTTON_MINUTE_PIN);
  if(minPbState == LOW && lastMinPbState == HIGH) {
    setMinute = (setMinute + 1) % 60;
    lastInteraction = millis(); // user pressed something
  }
  lastMinPbState = minPbState;
}

// ----------------------------------------------------------
//                   SHOW Date SET MODE
//   Display "dSET" for 2 seconds, then go into TIMESET
// ----------------------------------------------------------
void handleShowDSetMode()
{
  // Simply display "SET" for showSetTime (2s)
  sevseg.setChars("dSET");
  sevseg.refreshDisplay();

  if(currentMillis - hourPinPressTime >= showSetTime) {
    // After 2 seconds, enter TIMESET
    // Grab current RTC time as a starting point
    RtcDateTime now = Rtc.GetDateTime();
    setDay   = now.Day();
    setMonth = now.Month();

    watchState = DATESET;
    lastInteraction = millis(); // reset inactivity timer
  }
}

// ----------------------------------------------------------
//                   DATESET MODE
//   Press hour/minute buttons to set date. If no press for
//   5 seconds, lock in and sleep.
// ----------------------------------------------------------
void handleDateSetMode()
{
  // Display the 'setHour' and 'setMinute' being edited
  int dateCombined = (setDay * 100) + setMonth;
  sevseg.setNumber(dateCombined, 1);

  // If no interaction for 5s, commit the new time and sleep
  if(currentMillis - lastInteraction > wakeInterval) {
    RtcDateTime now = Rtc.GetDateTime();
    // Lock in the new time (day/month/year remain the same)
    Rtc.SetDateTime(RtcDateTime(now.Year(), setMonth, setDay, 
                                now.Hour(), now.Minute(), now.Second()));
    goToSleep();
    return;
  }

  // Check hour button
  hourPbState = digitalRead(BUTTON_HOUR_PIN);
  if(hourPbState == LOW && lastHourPbState == HIGH) {
    setDay = (setDay % 31) + 1;
    lastInteraction = millis(); // user pressed something
  }
  lastHourPbState = hourPbState;

  // Check minute button
  minPbState = digitalRead(BUTTON_MINUTE_PIN);
  if(minPbState == LOW && lastMinPbState == HIGH) {
    setMonth = (setMonth % 12) + 1;
    lastInteraction = millis(); // user pressed something
  }
  lastMinPbState = minPbState;
}

// ----------------------------------------------------------
//          Check if Spare Pin is held for 2 seconds
// ----------------------------------------------------------
void checkSparePinHold()
{
  int spareState = digitalRead(BUTTON_SPARE_PIN);

  // We assume pin is active LOW; adjust if reversed
  if(spareState == HIGH) {
    // If not previously pressed, note the time
    if(!sparePinHolding) {
      sparePinHolding = true;
      sparePinPressTime = currentMillis;
    } 
    else {
      // Already holding; check if 2s have elapsed
      if(!sparePinHeld && (currentMillis - sparePinPressTime >= holdDuration)) {
        // Officially move to SHOWSET
        sparePinHeld  = true;
        watchState    = SHOWSET;
        sparePinPressTime = currentMillis; // re-use to track "SET" display
      }
    }
  }
  else {
    // Spare pin released, reset flags
    sparePinHolding = false;
    sparePinHeld    = false;
  }
}

void checkHourPinHold()
{
  int dSetState = digitalRead(BUTTON_HOUR_PIN);

  // We assume pin is active LOW; adjust if reversed
  if(dSetState == HIGH) {
    // If not previously pressed, note the time
    if(!hourPinHolding) {
      hourPinHolding = true;
      hourPinPressTime = currentMillis;
    } 
    else {
      // Already holding; check if 2s have elapsed
      if(!hourPinHeld && (currentMillis - hourPinPressTime >= holdDuration)) {
        // Officially move to SHOWSET
        hourPinHeld  = true;
        watchState    = SHOWDSET;
        hourPinPressTime = currentMillis; // re-use to track "SET" display
      }
    }
  }
  else {
    // Spare pin released, reset flags
    hourPinHolding = false;
    hourPinHeld    = false;
  }
}


// ----------------------------------------------------------
//                   Sleep Logic
// ----------------------------------------------------------
void goToSleep()
{
  // Blank display & set brightness lower if desired
  sevseg.blank();
  sevseg.refreshDisplay();
  //sevseg.setBrightness(60);

  watchState = SLEEPING;

  // Example low-level sleep (on AVR):
  ADCSRA &= ~(1 << 7);      // Disable ADC
  SMCR |= (1 << 2);         // Power-down mode bit
  SMCR |= 1;                // Enable sleep
  MCUCR |= (3 << 5);        // BOD disable (bits 5 & 6)
  MCUCR = (MCUCR & ~(1 << 5)) | (1 << 6);
  __asm__ __volatile__("sleep");
}

// ----------------------------------------------------------
//                Interrupt for Wake Button
// ----------------------------------------------------------
void isrWake()
{
  // This is triggered by BUTTON_WAKE_PIN falling
  wakeInterruptTriggered = true;
  watchState = NORMAL;
  now = Rtc.GetDateTime();
  hour = now.Hour();
  minute = now.Minute();
  day = now.Day();
  month = now.Month();
  timeCombined = (hour * 100) + minute;
  dateCombined = (now.Day() * 100) + now.Month();
  lastInteraction = millis();
}