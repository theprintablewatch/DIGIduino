/*
  The Printable Watch 2024                                                                                
  theprintablewatch.com
*/

#include "SevSeg.h"
#include "RtcDS1302.h"

//RTC Set UP
ThreeWire myWire(14,15,16); // IO(DATA), SCLK(CLK), CE(RST)
RtcDS1302<ThreeWire> Rtc(myWire);

//SevSeg Startup
SevSeg sevseg; 

const byte BUTTON_PIN = 2;         // Wake up button
const byte BUTTON_HOUR_PIN = 18;    // Button to increment hour
const byte BUTTON_MINUTE_PIN = 19;
const byte BUTTON_SPARE_PIN = 0; 

volatile byte buttonPressed = 0;

int hourPbState = LOW;
int lastHourPbState = LOW;
int minPbState = LOW;
int lastminPbState = LOW;

unsigned long startMillis = 0;  // Stores the last time the timer was checked
const unsigned long interval = 5000; //Display on time in ms
unsigned long currentMillis = millis();


void setup() {
  //SevSeg Setup
  byte numDigits = 4;
  byte digitPins[] = {1, 3, 4, 5};
  byte segmentPins[] = {6, 7, 8, 9, 10, 11, 12, 13}; //changed 11 to 21 for debug
  bool resistorsOnSegments = false; // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_CATHODE; // See README.md for options - Changed to match PCB
  bool updateWithDelays = false; // Default 'false' is Recommended
  bool leadingZeros = true; // Use 'true' if you'd like to keep the leading zeros
  bool disableDecPoint = true; // Use 'true' if your decimal point doesn't exist or isn't connected
  
  //Wake Setup
  pinMode(BUTTON_PIN, INPUT); 
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), isrButton, FALLING);
  //Hour and Minute Buttons
  pinMode(BUTTON_HOUR_PIN, INPUT);
  pinMode(BUTTON_MINUTE_PIN, INPUT);
  pinMode(BUTTON_SPARE_PIN, INPUT);

  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays, leadingZeros, disableDecPoint); // Configuring the display
  sevseg.setBrightness(60); //Adjust if flicking

  Rtc.Begin(); // Start RTC
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  currentMillis = millis();
  while(currentMillis - startMillis <= interval) {
  sevseg.setChars("test"); //display TPW on screen
  sevseg.refreshDisplay();

  Rtc.SetDateTime(compiled);

  }
  
}

void loop() {

RtcDateTime now = Rtc.GetDateTime(); //Get the time from RTC

int sec = now.Second(); //Get Hour
int minute = now.Minute(); // Get Minute
int timeCombined = (minute * 100) + sec;


sec = now.Second(); //Get Hour
minute = now.Minute(); // Get Minute
timeCombined = (minute * 100) + sec;
sevseg.setNumber(timeCombined, 1);
sevseg.refreshDisplay(); // Must run repeatedlyd


while(digitalRead(BUTTON_PIN)==1)
{
 sevseg.setNumber(1111);
sevseg.refreshDisplay(); 
}


while(digitalRead(BUTTON_HOUR_PIN)==1)
{
 sevseg.setNumber(2222);
sevseg.refreshDisplay(); 
}


while(digitalRead(BUTTON_MINUTE_PIN)==1)
{
 sevseg.setNumber(3333);
sevseg.refreshDisplay(); 
}

while(digitalRead(BUTTON_SPARE_PIN)==1)
{
 sevseg.setNumber(4444);
sevseg.refreshDisplay(); 
}


  /////////////////////////////// 
 //           CLOCK           //
///////////////////////////////
/*
RtcDateTime now = Rtc.GetDateTime(); //Get the time from RTC


currentMillis = millis();

int hour = now.Hour(); //Get Hour
int minute = now.Minute(); // Get Minute
int timeCombined = (hour * 100) + minute;
  ////////////////////////////////////////////////////////// 
 //            Hour and Min setting functions            //
//////////////////////////////////////////////////////////


while(currentMillis - startMillis < interval)
{

currentMillis = millis();
 //formats the time for the display
 RtcDateTime now = Rtc.GetDateTime(); //Get the time from RTC
hour = now.Hour(); //Get Hour
minute = now.Minute(); // Get Minute
timeCombined = (hour * 100) + minute;
sevseg.setNumber(timeCombined, 1);
sevseg.refreshDisplay(); // Must run repeatedlyd


//Time Setting Function
minPbState = digitalRead(BUTTON_MINUTE_PIN);
if(minPbState == LOW && lastminPbState == HIGH){
minute = (minute + 1) % 60; // Increment hour by one per click
Rtc.SetDateTime(RtcDateTime(now.Year(), now.Month(), now.Day(), hour, minute, now.Second()));
timeCombined = (hour * 100) + minute;
}
lastminPbState = minPbState;

hourPbState = digitalRead(BUTTON_HOUR_PIN);
if(hourPbState == LOW && lastHourPbState == HIGH){
hour = (hour + 1) % 24; // Increment hour by one per click
Rtc.SetDateTime(RtcDateTime(now.Year(), now.Month(), now.Day(), hour, minute, now.Second()));
timeCombined = (hour * 100) + minute;
}
lastHourPbState = hourPbState;

}




  ////////////////////////////////////////// 
 //            SLEEP FUNCTION            //
//////////////////////////////////////////
if (currentMillis - startMillis >= interval) {
sevseg.blank(); // Clear the screen before sleeping
sevseg.refreshDisplay();
//LowPower.idle(SLEEP_FOREVER, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_OFF, TWI_OFF);
ADCSRA &= ~(1 << 7);
SMCR |= (1 <<2);
SMCR |= 1;

MCUCR |= (3 <<5);
MCUCR = (MCUCR & ~(1 <<5)) | (1 << 6);
__asm__ __volatile__("sleep");

}
*/
}




//Interupt for wake button
void isrButton()
{
  buttonPressed = 1;
  sevseg.setBrightness(100);
  startMillis = millis(); //starts timer for display on
}
