# How to program your DIGIduino

### You will need
  * Arduino UNO
  * Programmer Shield 
  * JST Cable
  * Arduino IDE installed on your device

## Step 1

### Arduino IDE setup

Before you start, add the MCUdude MiniCore board package to your directories.
File > Preferences - Additional boards manager URLs: > paste this link https://mcudude.github.io/MiniCore/package_MCUdude_MiniCore_index.json

Then head to Boards Manager, search for MiniCore by MCUdude and install

Now head to library manager and install the following libraries:

  * SevSeg - Dean Reading
  * Rtc by Makune - Micheael C. Miller

## Step 2

### Turn your UNO into a programmer

To use your arduino to program other MCUs you need to write a bit of code to it. Luckily, this code is presaved in your arduino IDE. 
File > Examples > 11.ArduinoISP > ArduinoISP

If you're using a standard Arduino UNO just upload this to your device without anything connected to it (aside from the USB of course)

Once that's uploaded, your arduino is ready to be used to program the watch! 

## Step 3

### Uploading Code

Open the DIGIduinoV1.ino code

Head to tools> Board > MiniCore > Atmega328

Make sure the rest of the settings under tools are as follows. 

One option which is really important is the Clock setting, MAKE SURE INTERNAL 8MHz is selected. if you burn a bootloader with an external crystal setting you may brick the device.

Connect the sheild to the UNO then plug your DIGIduino to the Shield using the supplied cable.
