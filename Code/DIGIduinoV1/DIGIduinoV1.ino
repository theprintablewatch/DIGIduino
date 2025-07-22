/*
  The Printable Watch 2025
  theprintablewatch.com

  Main Stable Code

  Battery test logic disabled
*/

#include "SevSeg.h"
//#include "LowPower.h"
#include "RtcDS1302.h"

// ------------------ RTC Setup ------------------
ThreeWire myWire(14, 15, 16);  // IO (DATA), SCLK (CLK), CE (RST)
RtcDS1302<ThreeWire> Rtc(myWire);

// ------------------ SevSeg Setup ------------------
SevSeg sevseg;

// ------------------ Times & Delays ------------------
const unsigned long WAKE_INTERVAL = 8000;  // Display active for 8s if no further interaction
const unsigned long COMMIT_TIME = 2000;    // Commit changes after 2s if no interaction
const unsigned long LONG_PRESS_DELAY = 2000;  // 2s hold to enter time-set mode
const unsigned long SHOW_SET_TIME = 2000;  // 2s to display "SET" before entering time-set
unsigned long currentMillis;
unsigned long lastInteraction = 0;         // Tracks when a button was last pressed

// ------------------ Buttons Pins ------------------

const byte BUTTON_WAKE_PIN = 2;    // Wake-up button (external interrupt)
const byte BUTTON_HOUR_PIN = 19;   // Hour increment button
const byte BUTTON_MINUTE_PIN = 18; // Minute increment button
const byte BUTTON_SPARE_PIN = 0;   // Enter time-set mode (held 2 seconds)

struct Button {
  const int pin;
  bool previouslyPressed;
  // pressed:
  bool pressed;
  bool pressedEvent; // true only for a single loop when pressed.
  unsigned long pressedEventTime;
  // long pressed:
  bool longPressed; // true only for a single loop when pressed.
  bool longPressedEvent;
};
struct Button wakeButton = {.pin = BUTTON_WAKE_PIN};
struct Button hourButton = {.pin = BUTTON_HOUR_PIN};
struct Button minuteButton = {.pin = BUTTON_MINUTE_PIN};
struct Button spareButton = {.pin = BUTTON_SPARE_PIN};

// ------------------ State Machine ------------------
enum WatchState {
  NORMAL,
  SHOWDATE,
  SHOWSET,
  TIMESET,
  SHOWDSET,
  DATESET,
  YEARSET,
  UKUSSET,
  SLEEPING
};

// ------------------- Moon Phases ------------------
const byte moonPhases[8][4] = {
  { 0x00, 0x00, 0x00, 0x00 },  // 0: New Moon
  { 0x00, 0x00, 0x00, 0x0F },  // 1: Waxing Crescent
  { 0x00, 0x00, 0x39, 0x0F },  // 2: First Quarter
  { 0x00, 0x39, 0x09, 0x0F },  // 3: Waxing Gibbous
  { 0x39, 0x09, 0x09, 0x0F },  // 4: Full Moon
  { 0x39, 0x09, 0x0F, 0x00 },  // 5: Waning Gibbous (fixed your typo)
  { 0x39, 0x0F, 0x00, 0x00 },  // 6: Last Quarter
  { 0x39, 0x00, 0x00, 0x00 }   // 7: Waning Crescent
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
  if (age < 0) {
    age += synodicMonth;
  }

  // Convert age to phase index 0–7
  byte index = (byte)((age / synodicMonth) * 8 + 0.5);  // round to nearest
  return index & 7;                                     // wrap to 0–7
}

long readVcc() {
  // Set up ADC to read internal 1.1V reference
  ADMUX = (1 << REFS0) | (1 << MUX3) | (1 << MUX2) | (1 << MUX1);  // MUX[3:0]=1110, REFS0=1
  delay(2);                                                        // Let Vref settle
  ADCSRA |= (1 << ADSC);                                           // Start conversion
  while (ADCSRA & (1 << ADSC))
    ;  // Wait until done
  int result = ADC;
  long vcc = 1125300L / result;  // 1.1V * 1023 * 1000 (to get mV)
  return vcc;                    // Vcc in millivolts
}

int voltageToPercentage(float voltage) {
  // Clamp voltage to expected range
  if (voltage >= 3.0) {
    return 100;
  } else if (voltage <= 1.8) {
    return 0;
  }

  // Piecewise linear approximation
  if (voltage > 2.9) {
    return 95 + (voltage - 2.9) * 50;  // 2.9–3.0V → 95–100%
  } else if (voltage > 2.7) {
    return 75 + (voltage - 2.7) * 100;  // 2.7–2.9V → 75–95%
  } else if (voltage > 2.2) {
    return 25 + (voltage - 2.2) * 100;  // 2.2–2.7V → 25–75%
  } else {
    return (voltage - 1.8) * 62.5;  // 1.8–2.2V → 0–25%
  }
}

WatchState watchState = NORMAL;


//static long vccMillivolts = readVcc();
//static int vccDisplay = vccMillivolts;

// ------------------ Global Time Variables -------------------
RtcDateTime now;
int hour = 0;
int minute = 0;
int timeCombined = 8888;
int day = 1;
int month = 1;
int year = 1066;
int dateCombined = 101;
int yearCombined = 2001;

bool UKUS = true;  // UK Date format DDMM = True, US Date Format MMDD = False

void setup() {
  // ------------------ Pin Modes ------------------
  buttonInit(&wakeButton);
  buttonInit(&hourButton);
  buttonInit(&minuteButton);
  buttonInit(&spareButton);
  pinMode(17, OUTPUT);

  // Attach interrupt for wake button
  attachInterrupt(digitalPinToInterrupt(BUTTON_WAKE_PIN), isrWake, FALLING);

  // ------------------ Seven-Segment Setup ------------------
  byte numDigits = 4;
  byte digitPins[] = { 1, 3, 4, 5 };
  byte segmentPins[] = { 6, 7, 8, 9, 10, 11, 12, 13 };
  bool resistorsOnSegments = false;      // 'false' → resistors on digit pins
  byte hardwareConfig = COMMON_CATHODE;  // On your PCB
  bool updateWithDelays = false;         // Recommended = false
  bool leadingZeros = true;              // Display leading zeros
  bool disableDecPoint = false;          // No decimal point used

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
  pinMode(20, OUTPUT);
  pinMode(21, OUTPUT);
  pinMode(22, OUTPUT);

  // Optional: briefly display "beta" (or "TPW") on startup
  unsigned long start = millis();
  static long vccMillivolts = readVcc();
  static int vccDisplay = vccMillivolts;
  //vccMillivolts = readVcc();
  //vccDisplay = vccMillivolts;

  while (millis() - start < 2000) {
    //sevseg.setChars("hey");
    // e.g., 4975
    // Convert to 4.97V → 497
    sevseg.setNumber(vccDisplay, 3);  // 2 decimal places (4.97)
  }
}

void loop() {
  currentMillis = millis();

  // Refresh the display often
  sevseg.refreshDisplay();

  // Refresh the buttons state
  buttonUpdateState(&wakeButton);
  buttonUpdateState(&hourButton);
  buttonUpdateState(&minuteButton);
  buttonUpdateState(&spareButton);
  if (wakeButton.pressed || hourButton.pressed || minuteButton.pressed || spareButton.pressed) {
    lastInteraction = currentMillis;
  }

  // State machine
  switch (watchState) {
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

    case YEARSET:
      handleYearSetMode();
      break;

    case UKUSSET:
      handleUKUSSetMode();
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
void handleNormalMode() {
  //check if min button is pressed, display date if it is
  if (minuteButton.pressedEvent) {
    watchState = SHOWDATE;
  } else if (spareButton.longPressedEvent) {
    watchState = SHOWSET;
  } else if (hourButton.longPressedEvent) {
    watchState = SHOWDSET;
  }

  //Time is read once globally before displaying time, reduces flickering
  sevseg.setNumber(timeCombined);

  // Check for inactivity → sleep
  if (currentMillis - lastInteraction > WAKE_INTERVAL) {
    goToSleep();
  }
}

// ----------------------------------------------------------
//                  DATE MODE - With moonphse
// ----------------------------------------------------------
void handleShowDateMode() {
  static unsigned long lastToggleTime = 0;
  //static bool showingDate = true;
  static int stateDate = 0;
  static bool firstEntry = true;
  static long vccMillivolts = readVcc();
  static int vccDisplay = vccMillivolts;
  static float voltage = vccDisplay / 1000.0;
  static int batPerc = 25;

  if (firstEntry) {
    now = Rtc.GetDateTime();  // Refresh only once on entry
    firstEntry = false;
  }

  // Toggle every 1000ms (1 second)
  unsigned long currentMillis = millis();
  if (currentMillis - lastToggleTime >= 1000) {
    stateDate = (stateDate + 1) % 3;
    lastToggleTime = currentMillis;
  }

  if (stateDate == 0) {
    //Moonphase
    byte moonPhase = getMoonPhase(now.Year(), now.Month(), now.Day());
    sevseg.setSegments(moonPhases[moonPhase]);

  } else if (stateDate == 1) {
    //Date
    if (UKUS == true) {
      dateCombined = (now.Day() * 100) + now.Month();
    } else {
      dateCombined = (now.Month() * 100) + now.Day();
    }

    sevseg.setNumber(dateCombined, 2);  // 1 = leading zeros

  } else if (stateDate == 2) {
    //Year
    yearCombined = now.Year();
    sevseg.setNumber(yearCombined);  // 1 = leading zeros

  } else if (stateDate == 3) {
    //Vcc
    sevseg.setNumber(vccDisplay, 3);

  } else if (stateDate == 4) {
    //battery percentage
    batPerc = voltageToPercentage(voltage);
    sevseg.setNumber(batPerc);
  }

  // Button released → return to NORMAL state
  if (!minuteButton.pressedEvent) {
    watchState = NORMAL;
  }
}

// ----------------------------------------------------------
//                   SHOWSET MODE
//   Display "SET" for 2 seconds, then go into TIMESET
// ----------------------------------------------------------
void handleShowSetMode() {
  static unsigned long stateEnteredTime = -1;
  if (stateEnteredTime == -1) {
    stateEnteredTime = currentMillis;
  }
  // Simply display "SET" for SHOW_SET_TIME (2s)
  sevseg.setChars("SET ");

  // After 2 seconds, enter TIMESET
  if (currentMillis - stateEnteredTime >= SHOW_SET_TIME) {
    watchState = TIMESET;
    stateEnteredTime = -1;
    lastInteraction = currentMillis;
  }
}

// ----------------------------------------------------------
//                   TIMESET MODE
//   Press hour/minute buttons to set time. If no press for
//   5 seconds, lock in and sleep.
// ----------------------------------------------------------
void handleTimeSetMode() {
  static int setHour = -1;
  static int setMinute = -1;

  if (setHour == -1 || setMinute == -1) {
    // Grab current RTC time as a starting point
    RtcDateTime now = Rtc.GetDateTime();
    setHour = now.Hour();
    setMinute = now.Minute();
  }

  // Display the 'setHour' and 'setMinute' being edited
  int timeCombined = (setHour * 100) + setMinute;
  sevseg.setNumber(timeCombined, 2);

  // If no interaction for 5s, commit the new time and sleep
  if (currentMillis - lastInteraction > COMMIT_TIME) {
    RtcDateTime now = Rtc.GetDateTime();
    // Lock in the new time (day/month/year remain the same)
    Rtc.SetDateTime(RtcDateTime(now.Year(), now.Month(), now.Day(),
                                setHour, setMinute, now.Second()));
    setHour = -1;
    setMinute = -1;
    goToSleep();
    return;
  }

  // Check buttons
  if (hourButton.pressedEvent) {
    setHour = (setHour + 1) % 24;
  }
  if (minuteButton.pressedEvent) {
    setMinute = (setMinute + 1) % 60;
  }
}

// ----------------------------------------------------------
//                   SHOW Date SET MODE
//   Display "dSET" for 2 seconds, then go into TIMESET
// ----------------------------------------------------------
void handleShowDSetMode() {
  static unsigned long stateEnteredDate = -1;
  if (stateEnteredDate == -1) {
    stateEnteredDate = currentMillis;
  }
  // Simply display "dSET" for SHOW_SET_TIME (2s)
  sevseg.setChars("dSET");

  if (currentMillis - stateEnteredDate >= SHOW_SET_TIME) {
    // After 2 seconds, enter DATESET
    watchState = DATESET;
    stateEnteredDate = -1;
    lastInteraction = currentMillis;
  }
}

// ----------------------------------------------------------
//                   DATESET MODE
//   Press hour/minute buttons to set date. If no press for
//   5 seconds, lock in and sleep.
// ----------------------------------------------------------
void handleDateSetMode() {
  static int setDay = -1;
  static int setMonth = -1;

  if (setDay == -1 || setMonth == -1) {
    // Grab current RTC time as a starting point
    RtcDateTime now = Rtc.GetDateTime();
    setDay = now.Day();
    setMonth = now.Month();
  }

  // Display the 'setDay' / 'setMonth' being edited
  if (UKUS == true) {
    dateCombined = (setDay * 100) + setMonth;
  } else {
    dateCombined = (setMonth * 100) + setDay;
  }
  sevseg.setNumber(dateCombined, 2);

  // If no interaction for 5s, commit the new time and sleep
  if (currentMillis - lastInteraction > COMMIT_TIME) {
    RtcDateTime now = Rtc.GetDateTime();
    // Lock in the new time (day/month/year remain the same)
    Rtc.SetDateTime(RtcDateTime(now.Year(), setMonth, setDay,
                                now.Hour(), now.Minute(), now.Second()));
    setDay = -1;
    setMonth = -1;
    watchState = YEARSET;
    return;
  }

  // Check buttons
  if (hourButton.pressedEvent) {
    setDay = (setDay % 31) + 1;
  }
  if (minuteButton.pressedEvent) {
    setMonth = (setMonth % 12) + 1;
  }
}

void handleYearSetMode() {
  static int setYear = -1;

  if (setYear == -1) {
    // Grab current RTC year as a starting point
    RtcDateTime now = Rtc.GetDateTime();
    setYear = now.Year();
  }

  // Display the 'setYear' being edited
  sevseg.setNumber(setYear);

  // If no interaction for 5s, commit the new time and sleep
  if (currentMillis - lastInteraction > COMMIT_TIME) {
    RtcDateTime now = Rtc.GetDateTime();
    // Lock in the new time (day/month/year remain the same)
    Rtc.SetDateTime(RtcDateTime(setYear, now.Month(), now.Day(),
                                now.Hour(), now.Minute(), now.Second()));
    setYear = -1;
    watchState = UKUSSET;
    return;
  }

  // Check buttons
  if (hourButton.pressedEvent) {
    setYear = setYear + 1;
  }
  if (minuteButton.pressedEvent) {
    setYear = setYear - 1;
  }
}

void handleUKUSSetMode() {
  // Display the 'setHour' and 'setMinute' being edited
  //static char dFormat[] = "DDMM";
  sevseg.setChars(UKUS ? "DDnn" : "nnDD");

  // If no interaction for 5s, commit the new time and sleep
  if (currentMillis - lastInteraction > COMMIT_TIME) {
    RtcDateTime now = Rtc.GetDateTime();
    // Lock in the new time (day/month/year remain the same)
    day = now.Day();
    month = now.Month();
    year = now.Year();
    if (UKUS == true) {
      dateCombined = (day * 100) + month;
    } else {
      dateCombined = (month * 100) + day;
    }
    yearCombined = year;

    now = Rtc.GetDateTime();
    watchState = NORMAL;
    return;
  }

  // Check buttons
  if (hourButton.pressedEvent || minuteButton.pressedEvent) {
    UKUS = !UKUS;
  }
}

// ----------------------------------------------------------
//                   Sleep Logic
// ----------------------------------------------------------
void goToSleep() {
  // Blank display & set brightness lower if desired
  sevseg.blank();
  sevseg.refreshDisplay();
  digitalWrite(17, LOW);
  //sevseg.setBrightness(60);

  watchState = SLEEPING;

  // Example low-level sleep (on AVR):
  ADCSRA &= ~(1 << 7);  // Disable ADC
  SMCR |= (1 << 2);     // Power-down mode bit
  SMCR |= 1;            // Enable sleep
  MCUCR |= (3 << 5);    // BOD disable (bits 5 & 6)
  MCUCR = (MCUCR & ~(1 << 5)) | (1 << 6);
  __asm__ __volatile__("sleep");
}

// ----------------------------------------------------------
//                Interrupt for Wake Button
// ----------------------------------------------------------
void isrWake() {
  // This is triggered by BUTTON_WAKE_PIN falling
  lastInteraction = millis();
  digitalWrite(17, HIGH);
  watchState = NORMAL;
  now = Rtc.GetDateTime();
  hour = now.Hour();
  minute = now.Minute();
  day = now.Day();
  month = now.Month();
  year = now.Year();
  if (UKUS == true) {
    dateCombined = (day * 100) + month;
  } else {
    dateCombined = (month * 100) + day;
  }
  timeCombined = (hour * 100) + minute;
  yearCombined = year;
}

// ----------------------------------------------------------
//                Button management
// ----------------------------------------------------------

void buttonInit(struct Button* button) {
  pinMode(button->pin, INPUT);
  button->previouslyPressed = false;
  button->pressed = false;
  button->pressedEvent = false;
  button->pressedEventTime = 0;
  button->longPressed = false;
  button->longPressedEvent = false;
}

void buttonUpdateState(struct Button* button) {
  button->previouslyPressed = button->pressed;
  // We assume pin is active HIGH; adjust if reversed
  button->pressed = (digitalRead(button->pin) == HIGH);
  if (!button->pressed) {
    // pin released, reset flags
    if (button->previouslyPressed) {
      button->previouslyPressed = false;
      button->pressedEventTime = 0;
      button->longPressedEvent = false;
    }

  } else if (button->pressedEvent) {
    // Already detected as a press.
    button->pressedEvent = false;

  } else if (!button->previouslyPressed) {
    // If not previously pressed, note the time
    button->pressedEvent = true;
    button->pressedEventTime = currentMillis;

  } else if (button->longPressedEvent) {
    // Already detected as a long press.
    button->longPressedEvent = false;

  } else if (!button->longPressed && currentMillis - button->pressedEventTime >= LONG_PRESS_DELAY) {
    button->longPressed = true;
    button->longPressedEvent = true;
  }
}
