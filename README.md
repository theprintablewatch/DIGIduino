# The Printable Watch Digital 
Check out [TPW](https://theprintablewatch.com)

Kickstarter Campaign [Kickstarter](https://www.kickstarter.com/projects/theprintablewatch/digiduino-arduino-based-diy-digital-watch-development-kit)

![](https://github.com/theprintablewatch/DIGIduino/blob/c6c6814bb03cd5c846c423ae425610ccb49aae05/Media/kickstarter%20Header.jpg)

## Features
  * Arduino based digital watch
    * ATMega328P based
    * DS1302 RTC
    * 4 Digit 7-segment display
    * 4 Programmable physical buttons
  * 3D printed watch case
  * 12 Month+ Battery life

## Arduino Requirements
 * Libraries Used:
   * SevSeg - Dean Reading - https://github.com/DeanIsMe/SevSeg
   * Rtc by Makuna - Michael C. Miller - https://github.com/Makuna/Rtc/wiki
   * MiniCore - MCUdude - https://github.com/MCUdude/MiniCore
      * Add this link to your Boards Manager under Preferences - https://mcudude.github.io/MiniCore/package_MCUdude_MiniCore_index.json

## 3D Printable Case

The case and pushbuttons are designed to be easily 3D printed from any standard 3D printer. I have made the .STEP files available as well as a .dxf of the PCB so you can modify the case or even design your own!
[Download Case Models](https://github.com/theprintablewatch/DIGIduino/tree/main/Models)
![](https://github.com/theprintablewatch/DIGIduino/blob/877531b2b9126e457ac04e760b6008f78bc14c94/Media/Explode%20View.jpg)

## How to use the watch?

[![Minimalist Arduino - How to Program ATmega328 and ATtiny85](https://img.youtube.com/vi/XvohYlr38eU/0.jpg)]((https://youtu.be/XvohYlr38eU))

## How to Program ATMega328

Follow the guide [here](https://github.com/theprintablewatch/DIGIduino/blob/541a8b5c67f3c3e3cfa8df9d08a5a50cb2cadccd/Programming%20Guide.MD)

I have also filmed a short youtube video explaining how to program a ATMega328 seperate from the Uno more generally.

[![Minimalist Arduino - How to Program ATmega328 and ATtiny85](https://img.youtube.com/vi/qGbuzuVSzVs/0.jpg)]((https://youtu.be/qGbuzuVSzVs))

To program the chip on the PCB, follow these steps using the JST Jumper cable

![Back Board](https://github.com/theprintablewatch/DIGIduino/blob/541a8b5c67f3c3e3cfa8df9d08a5a50cb2cadccd/Media/JST%20Header.jpg)

## Developing your own firmware

The DIGIduino firmware is open source, I encourage you to make a copy of the basic firmware and add your own features!
Pins used:

## 🔌 Pin Usage Summary

| **Pin** | **Arduino Alias** | **Function** |
|--------|-------------------|-------------|
| **0**   | `D0`              | Top Left Button |
| **1**   | `D1`              | SevSeg digit 0 control |
| **2**   | `D2`              | Top Right Button (Wake-up button) |
| **3**   | `D3`              | SevSeg digit 1 control |
| **4**   | `D4`              | SevSeg digit 2 control |
| **5**   | `D5`              | SevSeg digit 3 control |
| **6**   | `D6`              | SevSeg segment A |
| **7**   | `D7`              | SevSeg segment B |
| **8**   | `D8`              | SevSeg segment C |
| **9**   | `D9`              | SevSeg segment D |
| **10**  | `D10`             | SevSeg segment E |
| **11**  | `D11`             | SevSeg segment F |
| **12**  | `D12`             | SevSeg segment G |
| **13**  | `D13`             | SevSeg segment DP (decimal point) |
| **14**  | `A0`              | RTC Data I/O (`ThreeWire` – DS1302) |
| **15**  | `A1`              | RTC CLK (SCLK) |
| **16**  | `A2`              | RTC CE (RST) |
| **17**  | `A3`              | RTC Power Pin (set HIGH in `isrWake`, LOW in `goToSleep`) |
| **18**  | `A4`              | Low Left Button |
| **19**  | `A5`              | Low Right Button |
| **20**  | `20`                 | Spare - Disabled (`pinMode(20, OUTPUT)`) |
| **21**  | `21`                 | Spare - Disabled (`pinMode(21, OUTPUT)`) |
| **22**  | `22`                 | Spare - Disabled (`pinMode(22, OUTPUT)`) |


