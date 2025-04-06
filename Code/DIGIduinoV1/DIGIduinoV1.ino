
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
const unsigned long wakeInterval = 10000;    // Display active for 5s if no further interaction
const unsigned long holdDuration = 2000;    // 2s hold to enter time-set mode
const unsigned long showSetTime  = 2000;    // 2s to display "SET" before entering time-set

// ------------------ State Machine ------------------
enum WatchState {
  NORMAL,
  SHOWSET,
  TIMESET,
  SLEEPING
};

WatchState watchState = NORMAL;

// ------------------ Spare Pin Holding Logic ------------------
bool sparePinHeld        = false;
bool sparePinHolding     = false;
unsigned long sparePinPressTime = 0;

// ------------------ Hour/Minute Buttons ------------------
int hourPbState       = LOW;
int lastHourPbState   = LOW;
int minPbState        = LOW;
int lastMinPbState    = LOW;

// ------------------ Time Setting Variables ------------------
int setHour   = 0;
int setMinute = 0;

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

    case SHOWSET:
      handleShowSetMode();
      break;

    case TIMESET:
      handleTimeSetMode();
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
    //sevseg.setBrightness(100); // Possibly brighter upon waking
    lastInteraction = millis();
  }

  // Read time from RTC
  RtcDateTime now = Rtc.GetDateTime();
  int hour = now.Hour();
  int minute = now.Minute();

  // Display HHMM
  int timeCombined = (hour * 100) + minute;
  sevseg.setNumber(timeCombined, 1);

  // Check for inactivity → sleep
  if(currentMillis - lastInteraction > wakeInterval) {
    goToSleep();
    return;
  }

  // Handle Spare Pin (enter time-set if held 2s)
  checkSparePinHold();

  // We do not handle hour/minute increment in NORMAL mode, 
  // only in TIMESET mode after user holds the spare pin.
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

// ----------------------------------------------------------
//                   Sleep Logic
// ----------------------------------------------------------
void goToSleep()
{
  // Blank display & set brightness lower if desired
  sevseg.blank();
  sevseg.refreshDisplay();
  sevseg.setBrightness(60);

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
  lastInteraction = millis();
}
