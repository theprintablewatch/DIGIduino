/*
  The Printable Watch 2025
  theprintablewatch.com
*/

#include "SevSegMod2.h"
#include "RtcDS1302.h"

// ------------------ RTC Setup ------------------
ThreeWire myWire(14, 15, 16); // IO (DATA), SCLK (CLK), CE (RST)
RtcDS1302<ThreeWire> Rtc(myWire);

// ------------------ SevSeg Setup ------------------
SevSeg sevseg;

// ------------------ Pins ------------------
const byte BUTTON_WAKE_PIN   = 2;   // Wake-up button (external interrupt)
const byte BUTTON_HOUR_PIN   = 19;  // Hour increment button
const byte BUTTON_MINUTE_PIN = 18;  // Minute increment button
const byte BUTTON_MODE_PIN   = 0;   // Enter time-set mode (held 2 seconds)

// ------------------ Interrupt Flag ------------------
volatile bool wakeInterruptTriggered = false;

// ------------------ Times & Delays ------------------
unsigned long currentMillis      = 0;
unsigned long lastInteraction    = 0;
const unsigned long wakeInterval = 10000;
const unsigned long holdDuration = 2000;
const unsigned long showSetTime  = 2000;

// ------------------ State Machine ------------------
enum WatchState {
  NORMAL,
  SHOWSET,
  TIMESET,
  RANDOMGEN,
  SLEEPING
};

WatchState watchState = NORMAL;

// ------------------ Mode Pin Holding Logic ------------------
bool modePinHeld = false;
bool modePinHolding = false;
unsigned long modePinPressTime = 0;

// ------------------ Hour/Minute Buttons ------------------
int hourPbState = LOW;
int lastHourPbState = LOW;
int minPbState = LOW;
int lastMinPbState = LOW;

// ------------------ Time Setting Variables ------------------
int setHour = 0;
int setMinute = 0;

void setup()
{
  pinMode(BUTTON_WAKE_PIN, INPUT);
  pinMode(BUTTON_HOUR_PIN, INPUT);
  pinMode(BUTTON_MINUTE_PIN, INPUT);
  pinMode(BUTTON_MODE_PIN, INPUT);

  randomSeed(analogRead(A0));

  attachInterrupt(digitalPinToInterrupt(BUTTON_WAKE_PIN), isrWake, FALLING);

  byte numDigits = 4;
  byte digitPins[] = {1, 3, 4, 5};
  byte segmentPins[] = {6, 7, 8, 9, 10, 11, 12, 13};
  bool resistorsOnSegments = false;
  byte hardwareConfig = COMMON_CATHODE;
  bool updateWithDelays = false;
  bool leadingZeros = true;
  bool disableDecPoint = true;

  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins,
               resistorsOnSegments, updateWithDelays, leadingZeros,
               disableDecPoint);
  sevseg.setBrightness(5);

  Rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Rtc.SetDateTime(compiled);

  unsigned long start = millis();
  while (millis() - start < 2000) {
    sevseg.setChars("hey");
    sevseg.refreshDisplay();
  }

  lastInteraction = millis();
}

void loop()
{
  currentMillis = millis();
  sevseg.refreshDisplay();

  switch (watchState) {
    case NORMAL:
      handleNormalMode();
      break;
    case RANDOMGEN:
      handleRandomGenMode();
      break;
    case SHOWSET:
      handleShowSetMode();
      break;
    case TIMESET:
      handleTimeSetMode();
      break;
    case SLEEPING:
      break;
  }
}

// ----------------------------------------------------------
//                  NORMAL MODE
// ----------------------------------------------------------
void handleNormalMode()
{
  if (wakeInterruptTriggered) {
    wakeInterruptTriggered = false;
    lastInteraction = millis();
  }

  RtcDateTime now = Rtc.GetDateTime();
  int hour = now.Hour();
  int minute = now.Minute();
  int timeCombined = (hour * 100) + minute;
  sevseg.setNumber(timeCombined, 1);

  if (currentMillis - lastInteraction > wakeInterval) {
    goToSleep();
    return;
  }

  checkModePinHold();

  static bool minHolding = false;
  static unsigned long minHoldStart = 0;
  int minState = digitalRead(BUTTON_MINUTE_PIN);

  if (minState == HIGH) {
    if (!minHolding) {
      minHoldStart = millis();
      minHolding = true;
    } else if (millis() - minHoldStart >= 1000) { // faster 1s hold
      watchState = RANDOMGEN;
      lastInteraction = millis();
      minHolding = false;
      return;
    }
  } else {
    minHolding = false;
  }
}

// ----------------------------------------------------------
//                   SHOWSET MODE
// ----------------------------------------------------------
void handleShowSetMode()
{
  sevseg.setChars("SET ");

  if (currentMillis - modePinPressTime >= showSetTime) {
    RtcDateTime now = Rtc.GetDateTime();
    setHour = now.Hour();
    setMinute = now.Minute();

    watchState = TIMESET;
    lastInteraction = millis();
  }
}

// ----------------------------------------------------------
//                   TIMESET MODE
// ----------------------------------------------------------
void handleTimeSetMode()
{
  int timeCombined = (setHour * 100) + setMinute;
  sevseg.setNumber(timeCombined, 1);

  if (currentMillis - lastInteraction > wakeInterval) {
    RtcDateTime now = Rtc.GetDateTime();
    Rtc.SetDateTime(RtcDateTime(now.Year(), now.Month(), now.Day(),
                                setHour, setMinute, now.Second()));
    goToSleep();
    return;
  }

  hourPbState = digitalRead(BUTTON_HOUR_PIN);
  if (hourPbState == LOW && lastHourPbState == HIGH) {
    setHour = (setHour + 1) % 24;
    lastInteraction = millis();
  }
  lastHourPbState = hourPbState;

  minPbState = digitalRead(BUTTON_MINUTE_PIN);
  if (minPbState == LOW && lastMinPbState == HIGH) {
    setMinute = (setMinute + 1) % 60;
    lastInteraction = millis();
  }
  lastMinPbState = minPbState;
}

// ----------------------------------------------------------
//                   RANDOMGEN MODE
// ----------------------------------------------------------
void handleRandomGenMode()
{
  // Show "RND"
  unsigned long introStart = millis();
  while (millis() - introStart < 1000) {
    sevseg.setChars("RND ");
    sevseg.refreshDisplay();
  }

  // Rolling animation
  for (int i = 0; i < 10; i++) {
    sevseg.setNumber(random(1, 7));
    sevseg.refreshDisplay();
    delay(50);
  }

  int result = random(1, 7);
  sevseg.setNumber(result);

  // Show result for 3 seconds or exit early if MODE button is pressed
  unsigned long showStart = millis();
  while (millis() - showStart < 3000) {
    sevseg.refreshDisplay();

    if (digitalRead(BUTTON_MODE_PIN) == HIGH) {
      watchState = NORMAL;
      lastInteraction = millis();
      return;
    }
  }

  goToSleep();
}

// ----------------------------------------------------------
//          Check if Mode Pin is held for 2 seconds
// ----------------------------------------------------------
void checkModePinHold()
{
  int modeState = digitalRead(BUTTON_MODE_PIN);

  if (modeState == HIGH) {
    if (!modePinHolding) {
      modePinHolding = true;
      modePinPressTime = currentMillis;
    } else {
      if (!modePinHeld && (currentMillis - modePinPressTime >= holdDuration)) {
        modePinHeld = true;
        watchState = SHOWSET;
        modePinPressTime = currentMillis;
      }
    }
  } else {
    modePinHolding = false;
    modePinHeld = false;
  }
}

// ----------------------------------------------------------
//                   Sleep Logic
// ----------------------------------------------------------
void goToSleep()
{
  sevseg.blank();
  sevseg.refreshDisplay();
  sevseg.setBrightness(60);

  watchState = SLEEPING;

  ADCSRA &= ~(1 << 7);
  SMCR |= (1 << 2);
  SMCR |= 1;
  MCUCR |= (3 << 5);
  MCUCR = (MCUCR & ~(1 << 5)) | (1 << 6);
  __asm__ __volatile__("sleep");
}

// ----------------------------------------------------------
//                Interrupt for Wake Button
// ----------------------------------------------------------
void isrWake()
{
  wakeInterruptTriggered = true;
  watchState = NORMAL;
  lastInteraction = millis();
}
