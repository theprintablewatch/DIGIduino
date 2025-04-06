# The Printable Watch Digital 
Check out [TPW](https://theprintablewatch.com)

Kickstarter Campaign [Kickstater](https://www.kickstarter.com/projects/theprintablewatch/digiduino-arduino-based-diy-digital-watch-development-kit)

## Features
  * Arduino based digital watch
    * ATMega328P based
    * DS1302 RTC
    * 4 Digit 7-segment display
  * 3D printed watch case
  * 12 Month Battery life

## Current status of project
  * Recieved funding via Kicksarter
  * First batch of fully assembled PCBs on order

## Arduino Requirements
 * Librarys Used:
   * SevSeg - Dean Reading - https://github.com/DeanIsMe/SevSeg
   * Rtc by Makuna - Michael C. Miller - https://github.com/Makuna/Rtc/wiki
   * MiniCore - MCUdude - https://github.com/MCUdude/MiniCore
      * Add this link to your Boards Manager under Preferences - https://mcudude.github.io/MiniCore/package_MCUdude_MiniCore_index.json

## Purchase PCBs - Releasing soon

## [Download Case Models]

## How to Program ATMega328

I have uploaded a short youtube video explaining how to program a ATMega328 seperate from the Uno.

[![Minimalist Arduino - How to Program ATmega328 and ATtiny85](https://img.youtube.com/vi/qGbuzuVSzVs/0.jpg)]((https://youtu.be/qGbuzuVSzVs))

To program the chip on the PCB, follow these steps using the JST Jumper cable

![Back Board](https://github.com/theprintablewatch/TPWDigital/blob/main/Layouts/3.png?raw=true)

## Troublshooting

 * Segments aren't lighting up:
   * Check that none of the segments are shorting. Each segment pin is wired in parallel, meaning the segment "A" pin should be connected to the segment "A" pin on all of the digits. Also, double-check that all ATmega328 pins are properly connected to the PCB and not shorting with neighboring pins.
 * Time is not setting:
   * Check that there is voltage over the RTC pins. This should read your battery voltage of 3V or programmer voltage of 5V
   * Check that the switches are soldered correctly, try shorting the button pins on the PCB with something metallic
 * Segments aren't lighting up properly (look random)
   * Double check if you installed Common Cathode 7 segment digits. If not, change the "btye hardwareConfig = COMMON_CATHODE" to "btye hardwareConfig = COMMON_ANNODE" (around line 57).

