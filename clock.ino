/*
 * InternalClockSync.ino
 * example code illustrating time synced from a DCF77 receiver
 * Thijs Elenbaas, 2012-2017
 * This example code is in the public domain.

  This example shows how to fetch a DCF77 time and synchronize
  the internal clock. In order for this example to give clear output,
  make sure that you disable logging  from the DCF library. You can
  do this by commenting out   #define VERBOSE_DEBUG 1   in Utils.cpp.

  NOTE: If you used a package manager to download the DCF77 library,
  make sure have also fetched these libraries:

 * Time

 */

#include "DCF77.h"
#include "TimeLib.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define DCF_PIN 5         // Connection pin to DCF 77 device
#define DCF_INTERRUPT 0   // Interrupt number associated with pin
#define CONFIG_PIN 3      // Configuration switch pin
#define HOUR_MINUTE_PIN 4 // switch between hours or minutes setup
#define SCK_PIN A5        // Clock pin for OLED screen
#define SCA_PIN A4        // Data pin for OLED screen
#define SCREEN_WIDTH 128  // OLED display width,  in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_ADDRESS 0x3C // address of OLED screen
#define UP_PIN 2          // Connection pin to up button to increment minute, hours
#define DOWN_PIN 3        // Connection pin to down button to decrement minute, hours
#define RELAY_PIN 6       // pin to trigger the relay
#define POWER_CHECK_PIN 7 // pin to check if power supply in on
#define RELAY_DELAY 200   // how long the relay is open

time_t towerTime;
time_t DCFtime;
DCF77 DCF = DCF77(DCF_PIN, DCF_INTERRUPT);
bool configMode = false;
int configMinutes = 0;
bool towerTimeInitialized = false;
bool DCFTimeInitialized = false;

// declare an SSD1306 display object connected to I2C
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// run the controller setup
void setup()
{
  // initialize all times to 00:00:00 01.01.2000
  setTime(0, 0, 0, 0, 0, 2000);
  towerTime = setTime(0, 0, 0, 0, 0, 2000);
  DCFtime = setTime(0, 0, 0, 0, 0, 2000);

  Serial.begin(9600);
  DCF.Start();
  Serial.println("Waiting for DCF77 time ... ");
  Serial.println("It will take at least 2 minutes until a first update can be processed.");
  pinMode(UP_PIN, INPUT_PULLUP);   // Set up pin as input with internal pull-up resistor
  pinMode(DOWN_PIN, INPUT_PULLUP); // Set up pin as input with internal pull-up resistor
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(POWER_CHECK_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(UP_PIN), upChange, FALLING);     // Attach the interrupt
  attachInterrupt(digitalPinToInterrupt(DOWN_PIN), downChange, FALLING); // Attach the interrupt
  initializeOledDisplay();
}

// run the controller loop
void loop()
{
  int configMode = digitalRead(CONFIG_PIN);
  configMode ? runConfigurationMode() : runNormalOperation();
  checkNewDCFTimeAvailable();
  updateDisplay();
}

// initialize oled screen at startup
void initializeOledDisplay()
{
  // initialize OLED display with address 0x3C for 128x64
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    while (true)
      ;
  }
  delay(2000);         // wait for initializing
  oled.clearDisplay(); // clear display
}

// user switched to configuration mode to setup tower time
runConfigurationMode()
{
  configMode = true;
}

// running normal loop operation
void runNormalOperation()
{
  if (configMode)
  {
    // just switched from config mode
    // initialize some variables
    towerTimeInitialized = true;
  }
  configMode = false;
  // check if we need to increment tower time
  checkMinuteIncremented();
}

// check we need to increment tower time
void checkMinuteIncremented()
{
  // avoid turning the clock until tower clock and DCF time initialized
  if (towerTimeInitialized && DCFTimeInitialized)
  {
    int currentDayMinutes = getDayMinutes();        // using system time - get number of minutes in day for 12h format
    int towerDayMinutes = getDayMinutes(towerTime); // using tower time - get number of minutes in day  of tower time for 12h format
    // calculate minutes diff between controller time and tower time
    int minutesDiff = currentDayMinutes - towerDayMinutes;
    // minutesDiff > 0 curent time is ahead of tower time - try to catch up, but only if current time is less then 6 hours ahead, otherwise wait
    int sixHoursDiff = 6 * 60;
    if (minutesDiff > 0 && minutesDiff < sixHoursDiff)
    {
      // need to turn the clock
      if (turnTheClock())
      {
        //
      }
    }
  }
}

// try to turn the clock on tower
bool turnTheClock()
{
  // check we have power supply and not running on battery - when there is no power we can not run the motor
  if (isGridPowerOn())
  {
    // open relay
    digitalWrite(RELAY_PIN, HIGH);
    delay(RELAY_DELAY);
    // close relay
    digitalWrite(RELAY_PIN, LOW);
    // increment the tower time
    incrementTowerClock();
    return true;
  }
  return false;
}

// increment one minute of tower clock
incrementTowerClock()
{
  // get the current hour and minute on the tower
  int currentHour = hour(towerTime);
  int currentMinutes = minute(towerTime);
  // increment for 1 minute
  currentMinutes = currentMinutes + 1;
  // check there is no overflow
  if (currentMinutes == 60)
  {
    currentHour = currentHour + 1;
    currentMinutes = 0;
    if (currentHour == 24)
    {
      currentHour = 0
    }
  }
  // set new tower time
  towerTime = setTime(currentHour, currentMinutes, second(0), day(1), month(1), year(2000));
}

// function to check if we have power supply - reading pin value used to detect power supply
bool isGridPowerOn()
{
  int powerOn = digitalRead(POWER_CHECK_PIN);
  return powerOn == HIGH ? true : false;
}

// get number of minutes in a day by using only 12h format - because on tower we have only 12 hours
int getDayMinutes(time_t inputTime)
{
  int currentHour = hour(inputTime);
  int currentMinute = minute(inputTime);
  currentHour = currentHour % 12; // if hour is in 24h format is the same for tower clock to use 12 hour format
  return currentDayMinutes = currentHour * 60 + currentMinute;
}

// check if there is new DCF time available
void checkNewDCFTimeAvailable()
{
  time_t newDCFtime = DCF.getTime(); // Check if new DCF77 time is available
  if (newDCFtime != 0)
  {
    Serial.println("Time is updated");
    DCFtime = newDCFtime;
    setTime(DCFtime);
    DCFTimeInitialized = true; // until first DCF time not initialized prevent rotating the clock
  }
}

// update the Oled screen with all the data
void updateDisplay()
{
  oled.clearDisplay(); // clear OLED display
  displaySystemTime();
  displayDCFTime();
  displayTowerTime();
  displayPowerStatus();
  displayConfigStatus();
  oled.display();
}

// showing if controller is in config state
void displayConfigStatus()
{
  oled.setTextSize(1);      // text size
  oled.setTextColor(WHITE); // text color
  oled.setCursor(80, 40);   // position to display
  configMode ? oled.println("Config Mode: ON") : oled.println("Config Mode: OFF");
}

// display if we have power status
void displayPowerStatus()
{
  oled.setTextSize(1);      // text size
  oled.setTextColor(WHITE); // text color
  oled.setCursor(80, 5);    // position to display
  isGridPowerOn() ? oled.println("Power: ON") : oled.println("Power: OFF");
}

// display system time on display
void displaySystemTime()
{
  oled.setTextSize(1);                              // text size
  oled.setTextColor(WHITE);                         // text color
  oled.setCursor(0, 10);                            // position to display
  oled.println("System time:" + getFormatedTime()); // text to display
}

// display last DCF time
void displayDCFTime()
{
  oled.setTextSize(1);      // text size
  oled.setTextColor(WHITE); // text color
  oled.setCursor(0, 20);    // position to display
  if (DCFtime != 0)
  {
    oled.println("DCF time:" + getFormatedTime(DCFtime)); // text to display
  }
  else
  {
    oled.println("DCF time: Unknown"); // text to display
  }
}

// display current tower time
void displayTowerTime()
{
  oled.setTextSize(2);      // text size
  oled.setTextColor(WHITE); // text color
  oled.setCursor(0, 40);    // position to display

  if (configMode)
  {
    oled.println("Set tower time:" + getFormatedShortTime(towerTime)); // text to display
    configMinutes = digitalRead(HOUR_MINUTE_PIN);
    if (configMinutes)
    {
      // waiting to adjust the minutes
      display.drawLine(55, 60, 120, 64, WHITE); // Draw a line from (55,60) to (120,64)
    }
    else
    {
      // waiting to adjust the hours
      display.drawLine(5, 60, 50, 64, WHITE); // Draw a line from (5,60) to (50,64)
    }
  }
  else
  {
    oled.println("Tower time:" + getFormatedShortTime(towerTime)); // text to display
  }
}

// add leading zero if reguired and semicolon
String printDigits(int digits, bool addSemicolumn)
{
  // utility function for digital clock display: prints preceding colon and leading 0
  String output = "";
  if (addSemicolumn)
    output = ":";
  if (digits < 10)
    output = output + "0";
  output = output + String(digits);
  return output
}

// user incrementing hour or minute
void upChange()
{
  if (configMode)
  {
    if (configMinutes)
    {
      // up one minute
      // Get the current hour
      int currentMinute = minute(towerTime);

      // Increment the minutes
      currentMinute = (currentMinute + 1) % 60;

      // Update the time variable with the new hour
      towerTime = setTime(hour(towerTime), currentMinute, second(0), day(1), month(1), year(2000));
    }
    else
    {
      // up one hour
      // Get the current hour
      int currentHour = hour(towerTime);

      // Increment the hour (considering 24-hour format)
      currentHour = (currentHour + 1) % 24;

      // Update the time variable with the new hour
      towerTime = setTime(currentHour, minute(towerTime), second(0), day(1), month(1), year(2000));
    }
  }
}

// user decrementing hour or minute
void downChange()
{
  if (configMode)
  {
    if (configMinutes)
    {
      // down one minute
      // Get the current hour
      int currentMinute = minute(towerTime);

      // Decrement the minutes
      currentMinute = currentMinute - 1;
      if (currentMinute < 0)
      {
        currentMinute = 59;
      }

      // Update the time variable with the new hour
      towerTime = setTime(hour(towerTime), currentMinute, second(0), day(1), month(1), year(2000));
    }
    else
    {
      // down one hour
      // Get the current hour
      int currentHour = hour(towerTime);

      // Decrement the hour (considering 24-hour format)
      currentHour = currentHour - 1;
      if (currentHour < 0)
      {
        currentHour = 23;
      }

      // Update the time variable with the new hour
      towerTime = setTime(currentHour, minute(towerTime), second(0), day(1), month(1), year(2000));
    }
  }
}

// get formated input time with hours minutes and seconds
String getFormatedTime(time_t inputTime)
{
  return printDigits(inputTime.hour(), false) + printDigits(inputTime.minute(), true) + printDigits(inputTime.second(), true);
}

// get formated input date and time
String getFormatedDateTime(time_t inputTime)
{
  return printDigits(inputTime.day(), false) + "." + printDigits(inputTime.month(), false) + "." + printDigits(inputTime.year(), false) + ' ' + getFormatedTime(inputTime);
}

// display only hours and minutes
String getFormatedShortTime(time_t inputTime)
{
  return printDigits(inputTime.hour(), false) + printDigits(inputTime.minute(), true);
}