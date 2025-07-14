Full set-up instructions [here](https://github.com/theprintablewatch/DIGIduino/blob/main/Programming%20Guide.MD)

Before you start, add the MCUdude MiniCore board package to your directories.
File > Preferences - Additional boards manager URLs: > paste this link https://mcudude.github.io/MiniCore/package_MCUdude_MiniCore_index.json

Make sure to follow the programmin
  * SevSeg - Dean Reading
  * Rtc by Makuna - Micheael C. Miller

## Key Features and Libraries

The DIGIduino is an Arduino‑based digital watch built around:
- **ATmega328P** microcontroller
- **DS1302** real‑time clock (RTC)
- **Four-digit seven-segment display**
- **Four control buttons**

### Required Libraries
- [`SevSeg`](https://github.com/DeanIsMe/SevSeg)
- [`Rtc by Makuna`](https://github.com/Makuna/Rtc)

---

## Main Firmware File

The main sketch is located at:

Code/DIGIduinoV1/DIGIduinoV1.ino

The firmware uses a small **state machine** to manage different watch modes:

```c++
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
```

### Setup Function

The setup() function does the following:
 - Configures button pins
 - Attaches a wake-up interrupt
 - Initializes the seven-segment display
 - Starts the real-time clock
 - Loop and State Machine

The loop() function continuously refreshes the display and switches between states:

```c++
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
  // ... other cases
  case SLEEPING:
    // Nothing to do while asleep
    break;
}
```

### Example Mode: Normal Operation
handleNormalMode() handles standard time display and input:

```c++
// Time is read once globally to reduce flickering
sevseg.setNumber(timeCombined);

// Check for inactivity → sleep
if(currentMillis - lastInteraction > wakeInterval) {
  goToSleep();
  return;
}

// Handle Spare Pin (enter time-set if held 2s)
checkSparePinHold();
checkHourPinHold();
```

### Sleep and Wake

When idle, the watch enters low power sleep mode:
`pin 17` is the power pin to the RTC, setting this high and low can switch the RTC on and off. Note when the RTC is in sleep it will still keep track of time but it's not possible to set or read the time.

`digitalWrite(17, LOW)` puts the RTC to sleep

```c++
void goToSleep()
{
  // Blank display & reduce power
  sevseg.blank();
  sevseg.refreshDisplay();
  digitalWrite(17, LOW);
  watchState = SLEEPING;

  // Enter AVR power-down mode
  ADCSRA &= ~(1 << 7);
  SMCR |= (1 << 2);
  SMCR |= 1;
  MCUCR |= (3 << 5);
  MCUCR = (MCUCR & ~(1 << 5)) | (1 << 6);
  __asm__ __volatile__("sleep");
}
```

Waking is triggered by a button interrupt:
`digitalWrite(17, HIGH)` Wakes the RTC

```c++
void isrWake()
{
  wakeInterruptTriggered = true;
  digitalWrite(17, HIGH);
  watchState = NORMAL;

  now = Rtc.GetDateTime();
  hour = now.Hour();
  minute = now.Minute();
  day = now.Day();
  month = now.Month();
  year = now.Year();

  lastInteraction = millis();
}
```

## Getting Started

Install the MiniCore board package and required libraries (see README.md and Programming Guide.MD).
Open DIGIduinoV1.ino in the Arduino IDE.
Review the state machine and handlers.
Try modifying handleShowDateMode() or add a new watch mode.
Use HardwareTest/HardwareTest.ino to test buttons and display.

## Firmware Architecture Summary
The firmware is structured with:
Initialization in setup()
A state machine in loop()
Dedicated handler functions for each mode
This clean separation makes it easy to understand and extend. Once you're comfortable with how state transitions and display updates work, you can start adding custom watch features.

