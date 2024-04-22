/*
  Control church tower clock with arduino microcontroller using Real Time Clock (RTC) Module - always try to synchronize tower clock time with controller system time
  Pause tower clock if there is no grid power that can rotate the motor
  Taking into consideration also rules for Daylight Saving Time (DST) in Europe
  Using rotary encoder to setup all variables
  Using Lithium battery as a controller UPS
  Reading and recording min/max temperature from RTC module

  Avgustin Tomsic
  March 2024
*/

//#include <DS3231.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "RTClib.h"
#include <Bounce2.h>

// rotary encoder pins
#define CLK_PIN 2  // rotary encoder clock pin
#define SW_PIN 3   // rotary encoder switch pin
#define DT_PIN 8   // rotary encoder data pin

// OLED display options
#define SCREEN_WIDTH 128     // OLED display width, in pixels
#define SCREEN_HEIGHT 64     // OLED display height, in pixels
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

// other pins
#define POWER_CHECK_PIN 10  // pin for checking power supply
#define RELAY_1_PIN 14      // define pin to trigger the relay 1
#define RELAY_2_PIN 15      // define pin to trigger the relay 2

#define MOTOR_DELAY 5000           // motor delay betwen next time changing state
#define NO_ACTIVITY_DURATION 60000  // the duration of waiting time in edit mode after which we auto close edit mode without changes

// system time data
uint16_t year = 2024;
uint8_t month = 1;
uint8_t day = 1;
uint8_t hour = 0;
uint8_t minute = 0;
uint8_t second = 0;
uint8_t dayOfWeek = 1;
bool isSummerTime = false;  // are we using summer or winter time depending on DST rules

// tower time - the current position of tower clock indicators
uint8_t towerHour = 0;
uint8_t towerMinute = 0;

bool channel1Open = false;  // next state of relay
/* 32-bit times as seconds since 2000-01-01. */
uint32_t currentSeconds;
uint32_t lastUpdateSeconds;
unsigned long lastUpdateEditModeScreen = 0;  // when motor started to rotate
DateTime currentTime;
//DateTime //lastTimeStartup;
bool editMode = false;
bool editModeBlinkDark = false;
unsigned long lastEditModeChange = 0;     // stored time when user done some interaction in edit mode - auto cancel edit mode after timeout
uint8_t mainScreenIndex = 1;

// motor
bool motorRotating = false;        // current state of the motor - true: rotating, false: motor not rotating any more
unsigned long motorOpenStart = 0;  // when motor started to rotate


// temperature
int8_t lastTemp;
int8_t minTemp = 120;
int8_t maxTemp = -120;
DateTime minTempDate;
DateTime maxTempDate;

// RTC module self compensation
uint16_t compensateAfterDays = 0;
int8_t compensateSeconds = 0;  // possible values -1s, 0 or +1s
unsigned long secondsElapsedSinceLastCompensation = 0;
unsigned long secondsToCompensate = 0;
//DateTime lastTimeCompensated;
short totalSecondsCompensated = 0;

// setup mode variables
uint8_t currentPageIndex;
// 1 - day
// 2 - month
// 3 - year
// 4 - hour
// 5 - minute
// 6 - tower hour
// 7 - tower minute
// 8 - compensate after days
// 9 - compensate seconds
// 10 - reset arduino
// 11 - confirmation setings
// 12 - cancel settings
uint8_t selectedPageIndex;
uint16_t edit_year;
uint8_t edit_month;
uint8_t edit_day;
uint8_t edit_hour;
uint8_t edit_minute;
uint8_t edit_towerHour;
uint8_t edit_towerMinute;
uint16_t edit_compensateAfterDays;
int8_t edit_compensateSeconds;
//DateTime lastTimeSetup;


// initialize objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RTC_DS3231 myRTC;
Bounce debouncerSwitch = Bounce();  // Create a Bounce object to prevent any glitches from rotary encoder switch
Bounce debouncerClk = Bounce();     // Create a Bounce object to prevent any glitches from rotary encoder clock signal

// initialize the controller
void setup() {
  Serial.begin(9600);
  delay(1000);  // wait for console opening
  if (!myRTC.begin()) {
    Serial.println(F("Couldn't find RTC"));
    while (1)
      ;
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }
  // setup controller pins
  pinMode(CLK_PIN, INPUT);
  pinMode(DT_PIN, INPUT);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(POWER_CHECK_PIN, INPUT);
  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT);
  digitalWrite(RELAY_1_PIN, HIGH);  // turn on the relay 1
  digitalWrite(RELAY_2_PIN, LOW);   // turn off the relay 2
  debouncerSwitch.attach(SW_PIN);   // Attach to the rotary encoder press pin
  debouncerSwitch.interval(2);      // Set debounce interval (in milliseconds)
  debouncerClk.attach(CLK_PIN);     // Attach to the rotary encoder clock pin
  debouncerClk.interval(2);         // Set debounce interval (in milliseconds)

  getCurrentTime();
  //lastTimeStartup = currentTime;
  checkTemperature();
  isSummerTime = checkIsSummerTime();  // check if currently is Summer or Winter time depending to DST rules
  // initialize tower clock on same as current time  - will be set correctly in edit mode - here just to prevent rotating the tower clock
  towerHour = hour;
  towerMinute = minute;
  display.clearDisplay();
  display.display();  // this command will display all the data which is in buffer
  display.setTextWrap(false);

  Serial.println(F("Initialization end"));
}

// main microcontroller loop
void loop() {
  // Update the debouncer
  debouncerSwitch.update();
  debouncerClk.update();

  // Read debounced rotary encoder press button state
  if (debouncerSwitch.fell()) {
    encoderPressed();
  }

  // Read debounced rotary encoder rotating clock event
  if (debouncerClk.fell()) {
    encoderRotated();
  }

  getCurrentTime();  // read current time from RTC module
  if (editMode) {
    if (checkTimeoutExpired(lastUpdateEditModeScreen, 300)) {
      editModeBlinkDark = !editModeBlinkDark;
      lastUpdateEditModeScreen = millis();
      updateEditModeDisplay();
    }
    // check auto exit edit mode timeout already passed
    if (checkTimeoutExpired(lastEditModeChange, NO_ACTIVITY_DURATION)) {
      exitEditMode();  // auto exit edit mode after no interaction timeout
    }
  } else {
    if (lastUpdateSeconds != currentSeconds) {
      lastUpdateSeconds = currentSeconds;
      updateTowerClock();
      updateMainScreen();
      checkNeedToCompensateTime();
      if (checkTimeoutExpired(lastEditModeChange, NO_ACTIVITY_DURATION)) {
        mainScreenIndex = 0;  // close display
      }
    }
  }
  checkCloseMotorRelay();  // check rotating delay expired
}

// reset function to have ability to reset Arduino programmatically
void (*resetFunc)(void) = 0;

// read the current time from the RTC module
void getCurrentTime() {
  currentTime = myRTC.now();
  /* 32-bit times as seconds since 2000-01-01. */
  currentSeconds = currentTime.secondstime();
  year = currentTime.year();
  month = currentTime.month();
  day = currentTime.day();
  hour = currentTime.hour();
  minute = currentTime.minute();
  second = currentTime.second();
  dayOfWeek = currentTime.dayOfTheWeek();

  //check time need to be updated because of the DST rules
  checkDaylightSavingChanges();
}

// update the tower clock if minute incremented
void updateTowerClock() {
  int currentDayMinutes = getSystemMinutes();               // using system time - get number of minutes in day for 12h format
  int towerDayMinutes = getTowerMinutes();                  // using tower time - get number of minutes in day  of tower time for 12h format
  short minutesDiff = currentDayMinutes - towerDayMinutes;  // calculate minutes diff between controller time and tower time
  // minutesDiff > 0 curent time is ahead of tower time - try to catch up, but only if current time is less then 10 hours ahead, otherwise wait
  short sixHoursDiff = 10 * 60;
  if (minutesDiff > 0 && minutesDiff < sixHoursDiff) {
    turnTheClock();      // need to turn the tower clock
    checkTemperature();  // every minute read also RTC module temperature
  }
}

// Turn the tower clock if there is power supply from grid, because otherwise motor will not rotate when relay is triggered
void turnTheClock() {
  // check we have power supply and not running on battery - when there is no power we can not run the motor
  // check also not already in motor rotating state
  if (motorRotating == false && isGridPowerOn()) {
    incrementTowerClock();                    // increment the tower time fist to display correct new time on lcd screen
    motorRotating = true;                     // set variable that relay is open and motor is in rotating state
    motorOpenStart = millis();                // store time when we start rotating the motor to stop after timeout
    digitalWrite(RELAY_1_PIN, channel1Open);  // internate both relay between  different states
    channel1Open = !channel1Open;             // toggle the state
    digitalWrite(RELAY_2_PIN, channel1Open);  // internate both relay between different states
  }
}

// increment one minute of tower clock
void incrementTowerClock() {
  // increment for 1 minute
  towerMinute = towerMinute + 1;
  // check there is no overflow
  if (towerMinute == 60) {
    towerHour = towerHour + 1;
    towerMinute = 0;
    if (towerHour >= 24) {
      towerHour = 0;
    }
  }
}

// check if we need to close the relay or motor stopped rotating
// calculate time difference instead of using the delays
// do not trigger the relay for next 5 seconds - to not rotate too quickly when hour change on DST also motor is rotating longer then relay is open - motor is having own relay to turn off when rotated enough
void checkCloseMotorRelay() {
  if (motorRotating && checkTimeoutExpired(motorOpenStart, MOTOR_DELAY)) {
    motorRotating = false;
  }
}

// read the current temperature from the RTC module - the precision of the temperature sensor is ± 3°C
void checkTemperature() {
  // read current temperature
  lastTemp = myRTC.getTemperature();
  // check current temperature exceeds max recorded temp
  if (lastTemp > maxTemp) {
    maxTemp = lastTemp;
    maxTempDate = currentTime;
  }
  // check current temp is lower them min recorded temp
  if (lastTemp < minTemp) {
    minTemp = lastTemp;
    minTempDate = currentTime;
  }
}


// common function to check if different timeouts already expired from last change
bool checkTimeoutExpired(unsigned long lastChange, unsigned int timeoutDuration) {
  unsigned long currentMillis = millis();
  return ((currentMillis - lastChange) >= timeoutDuration);
}


// get number of minutes in a day by using only 12h format - because on tower we have only 12 hours
short getSystemMinutes() {
  short currentHour = hour % 12;  // if hour is in 24h format is the same for tower clock to use 12 hour format
  return currentHour * 60 + minute;
}

// get number of minutes of tower clock in a day by using only 12h format
short getTowerMinutes() {
  short currentHour = towerHour % 12;  // if hour is in 24h format is the same for tower clock to use 12 hour format
  short dayMinutes = currentHour * 60 + towerMinute;
  if (dayMinutes == 719) {  // if last minute in a day then return -1, because first minute in a day using system time will be zero, otherwise tower clock will not be incremented
    dayMinutes = -1;
  }
  return dayMinutes;
}

void updateMainScreen() {
  display.fillRect(0, 0, 128, 64, SSD1306_BLACK);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  if (mainScreenIndex == 1) {
    addStatusInfo();
    display.setTextSize(1);
    display.setCursor(0, 22);
    display.setTextColor(WHITE);
    char currentDate[16];
    sprintf(currentDate, "%02d.%02d.%4d", day, month, year);  //add leading zeros to the day and month
    display.print(currentDate);
    display.setCursor(70, 22);
    display.print(getWeekDayName(dayOfWeek));
    char currentTime[16];
    sprintf(currentTime, "%02d:%02d:%02d ", hour, minute, second);
    display.setTextSize(2);
    display.setCursor(0, 34);
    display.print(currentTime);
    display.setTextSize(1);
    display.setCursor(0, 56);
    char towerTime[8];
    sprintf(towerTime, "%02d:%02d ", towerHour, towerMinute);
    display.print(towerTime);
    if (motorRotating) {
      display.setCursor(50, 56);
      display.print(F("Rotating"));
    }
  } else if (mainScreenIndex == 2) {
    // temperature details
    display.setCursor(0, 0);
    display.print(F("Temperature"));
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.print(getTemp(lastTemp));
    display.setTextSize(1);
    display.setCursor(0, 27);
    display.print(F("Min:"));
    display.setCursor(66, 27);
    display.print(getTemp(minTemp));
    display.setCursor(0, 37);
    char minTempDateTime[24];
    sprintf(minTempDateTime, "%02d.%02d.%4d %02d:%02d", minTempDate.day(), minTempDate.month(), minTempDate.year(), minTempDate.hour(), minTempDate.minute());
    display.print(minTempDateTime);
    display.setCursor(0, 47);
    display.print(F("Max:"));
    display.setCursor(66, 47);
    display.print(getTemp(maxTemp));
    display.setCursor(0, 57);
    char maxTempDateTime[24];
    sprintf(maxTempDateTime, "%02d.%02d.%4d %02d:%02d", maxTempDate.day(), maxTempDate.month(), maxTempDate.year(), maxTempDate.hour(), maxTempDate.minute());
    display.print(maxTempDateTime);
  } else if (mainScreenIndex == 3) {
    // // time compensation data
    // display.setCursor(0, 0);
    // display.print(F("Compensate after"));
    // display.setCursor(0, 10);
    // display.print(compensateAfterDays);
    // display.setCursor(30, 10);
    // display.print(F("days"));
    // display.setCursor(60, 10);
    // display.print(compensateSeconds);
    // display.setCursor(80, 10);
    // display.print(F("seconds"));
    // display.setCursor(0, 20);
    // display.print(F("Last compensation:"));
    // display.setCursor(0, 30);
    // // display last date time that clock has been self adjusted

    // if (totalSecondsCompensated == 0) {
    //   display.print(F("Never"));
    // } else {
    //   char lastCompensate[24];
    //   sprintf(lastCompensate, "%02d.%02d.%4d %02d:%02d:%02d", //lastTimeCompensated.day(), //lastTimeCompensated.month(), //lastTimeCompensated.year(), //lastTimeCompensated.hour(), //lastTimeCompensated.minute(), //lastTimeCompensated.second());
    //   display.print(lastCompensate);
    // }
    // display.setCursor(0, 40);
    // display.print(F("Total compensated:"));
    // display.setCursor(0, 50);
    // display.print(totalSecondsCompensated);
    // display.setCursor(40, 50);
    // display.print(F("seconds"));

    //
    // display.print(F("Last Startup"));
    // display.setCursor(0, 10);
    // char lastStartupDateTime[24];
    // sprintf(lastStartupDateTime, "%02d.%02d.%4d %02d:%02//d:%02d", lastTimeStartu//p.day(), lastTimeStartup.//month(), lastTimeStartup//.year(), lastTimeStartup//.hour(), lastTimeStartup.m//inute(), lastTimeStartup.second());
    // display.print(lastStartupDateTime);
    // display.setCursor(0, 20);
    // display.print(F("Last Setup"));
    // display.setCursor(0, 30);
    // char lastSetupDateTime[24];
    // sprintf(lastSetupDateTime, "%02d.%02d.%4d %02d:%02d:%02d", lastTimeSetup.day(), //lastTimeSetup.month(), //lastTimeSetup.year(), //lastTimeSetup.hour(), //lastTimeSetup.minute(), //lastTimeSetup.second());
    // display.print(lastSetupDateTime);
  }
  display.display();
}

// display status bar information
void addStatusInfo() {
  display.setTextSize(1);
  // display power status
  display.setCursor(0, 0);
  isGridPowerOn() ? display.print(F("Power: OK")) : display.print(F("Power: Fail"));
  // display temperature
  display.setCursor(80, 0);
  display.print(getTemp(lastTemp));
  // display Summer/Winter Time
  display.setCursor(0, 10);
  display.print(isSummerTime ? F("Summer") : F("Winter"));
  // display open SSR channel
  display.setCursor(80, 10);
  display.print(channel1Open ? F("CH 1") : F("CH 2"));
}

void updateEditModeDisplay() {
  display.fillRect(0, 0, 128, 64, SSD1306_BLACK);
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("System:"));
  display.setCursor(68, 0);
  char editDate[16];
  sprintf(editDate, "%02d.%02d.%4d", edit_day, edit_month, edit_year);  //add leading zeros to the day and month
  display.print(editDate);
  display.setCursor(68, 12);
  char editTime[16];
  sprintf(editTime, "%02d:%02d:%02d ", edit_hour, edit_minute, 0);
  display.print(editTime);
  display.setCursor(0, 24);
  display.print(F("Tower:"));
  display.setCursor(68, 24);
  char towerTime[16];
  sprintf(towerTime, "%02d:%02d ", edit_towerHour, edit_towerMinute);
  display.print(towerTime);
  display.setCursor(0, 36);
  display.print(F("Compensate"));
  display.setCursor(68, 36);
  display.print(edit_compensateAfterDays);
  display.setCursor(90, 36);
  display.print(F("d"));
  display.setCursor(100, 36);
  display.print(edit_compensateSeconds);
  display.setCursor(112, 36);
  display.print(F("s"));
  display.setCursor(0, 54);
  display.print(F("Reset"));
  display.setCursor(40, 54);
  display.print(F("Confirm"));
  display.setCursor(90, 54);
  display.print(F("Cancel"));
  // no item selected to edit  - draw line under option
  if (currentPageIndex != selectedPageIndex) {
    // 1 - day
    // 2 - month
    // 3 - year
    // 4 - hour
    // 5 - minute
    // 6 - tower hour
    // 7 - tower minute
    // 8 - compensate after days
    // 9 - compensate seconds
    // 10 - reset arduino
    // 11 - confirmation setings
    // 12 - cancel settings
    switch (currentPageIndex) {
      case 1:  // day
        display.drawFastHLine(68, 8, 12, WHITE);
        break;
      case 2:  // month
        display.drawFastHLine(85, 8, 12, WHITE);
        break;
      case 3:  // year
        display.drawFastHLine(104, 8, 24, WHITE);
        break;
      case 4:  // hour
        display.drawFastHLine(68, 20, 12, WHITE);
        break;
      case 5:  // minute
        display.drawFastHLine(85, 20, 12, WHITE);
        break;
      case 6:  // tower hour
        display.drawFastHLine(68, 32, 12, WHITE);
        break;
      case 7:  // tower minute
        display.drawFastHLine(85, 32, 12, WHITE);
        break;
      case 8:  // compensate after days
        display.drawFastHLine(68, 44, 18, WHITE);
        break;
      case 9:  // compensate seconds
        display.drawFastHLine(100, 44, 10, WHITE);
        break;
      case 10:  // reset
        display.drawFastHLine(0, 63, 30, WHITE);
        break;
      case 11:  // confirm
        display.drawFastHLine(40, 63, 41, WHITE);
        break;
      case 12:  // cancel
        display.drawFastHLine(90, 63, 35, WHITE);
        break;
    }
  } else {
    // one of options selected to edit
    if (editModeBlinkDark) {
      switch (currentPageIndex) {
        case 1:  // day
          display.fillRect(68, 0, 12, 10, SSD1306_BLACK);
          break;
        case 2:  // month
          display.fillRect(85, 0, 12, 10, SSD1306_BLACK);
          break;
        case 3:  // year
          display.fillRect(104, 0, 24, 10, SSD1306_BLACK);
          break;
        case 4:  // hour
          display.fillRect(68, 12, 12, 10, SSD1306_BLACK);
          break;
        case 5:  // minute
          display.fillRect(85, 12, 12, 10, SSD1306_BLACK);
          break;
        case 6:  // tower hour
          display.fillRect(68, 24, 12, 10, SSD1306_BLACK);
          break;
        case 7:  // tower minute
          display.fillRect(85, 24, 12, 10, SSD1306_BLACK);
          break;
        case 8:  // compensate after days
          display.fillRect(68, 36, 18, 10, SSD1306_BLACK);
          break;
        case 9:  // compensate seconds
          display.fillRect(100, 36, 10, 10, SSD1306_BLACK);
          break;
      }
    }
  }
  display.display();
}

// format temperature with degrees symbol
String getTemp(int8_t temp) {
  return String(temp) + " " + (char)247 + F("C");
}

// function to check if we have power supply - reading pin value used to detect power supply
bool isGridPowerOn() {
  return digitalRead(POWER_CHECK_PIN);
}

// calculate current date is using Summer or Winter DST rules
bool checkIsSummerTime() {
  // earliest possible date
  // 25 26 27 28 29 30 31 - date
  // 7  1  2  3  4  5  6  - day of week index

  // latest possible date
  // 25 26 27 28 29 30 31
  // 1  2  3  4  5  6  7

  if (month < 3 || month > 10)
    return false;
  if (month > 3 && month < 10)
    return true;

  // calculate previous sunday day
  int previousSunday = day - dayOfWeek % 7;  // use different indexes the app 0 - Sunday, 1-Monday,..., 6-Saturday

  if (month == 3)
    return previousSunday >= 25;
  if (month == 10)
    return previousSunday < 25;

  return false;  // this line never gonna happened
}

// user presses encoder button
void encoderPressed() {
  lastEditModeChange = millis();
  if (editMode) {
    if (currentPageIndex == 10) {
      // reset
      resetFunc();
    } else if (currentPageIndex == 11) {
      // confirm
      // finishing the edit mode - update the RTC module with new settings
      myRTC.adjust(DateTime(edit_year, edit_month, edit_day, edit_hour, edit_minute, 0));
      towerHour = edit_towerHour;
      towerMinute = edit_towerMinute;
      compensateAfterDays = edit_compensateAfterDays;
      compensateSeconds = edit_compensateSeconds;
      secondsElapsedSinceLastCompensation = 0;
      secondsToCompensate = compensateAfterDays * 86400;
      totalSecondsCompensated = 0;
      isSummerTime = checkIsSummerTime();  //
      //lastTimeSetup = myRTC.now();
      exitEditMode();  // move to normal operational mode
    } else if (currentPageIndex == 12) {
      // cancel
      exitEditMode();
    } else {
      if (currentPageIndex != selectedPageIndex) {
        selectedPageIndex = currentPageIndex;
      } else {
        selectedPageIndex = 0;
      }
    }
  } else {
    // currently not in edit mode and button pressed
    enterEditMode();
    updateEditModeDisplay();
  }
}

// event when user is rotating the rotary encoder button
void encoderRotated() {
  lastEditModeChange = millis();
  int change = 0;
  int dtState = digitalRead(DT_PIN);
  if (dtState == HIGH) {
    change = 1;  // rotated clockwise
  } else {
    change = -1;  // rotated anticlockwise
  }
  if (editMode) {  // edit mode
    if (currentPageIndex != selectedPageIndex) {
      currentPageIndex = currentPageIndex + change;
      currentPageIndex = checkRange(currentPageIndex, 1, 12);
    } else {
      switch (currentPageIndex) {
        case 1:  // day
                 // update the system date day
          edit_day = edit_day + change;
          edit_day = checkRange(edit_day, 1, 31);
          break;
        case 2:  // month
                 // update the system time month
          edit_month = edit_month + change;
          edit_month = checkRange(edit_month, 1, 12);
          break;
        case 3:  // year
                 // update the system time year - changing only last 2 digits
          edit_year = edit_year + change;
          edit_year = checkRange(edit_year, 2024, 2099);  // minimum year can be current year 2024
          break;
        case 4:  // hour
                 // update the system time hour
          edit_hour = edit_hour + change;
          edit_hour = checkRange(edit_hour, 0, 23);
          break;
        case 5:  // minute
                 // update the system time minutes
          edit_minute = edit_minute + change;
          edit_minute = checkRange(edit_minute, 0, 59);
          break;
        case 6:  // tower hour
                 // updating the tower clock hour
          edit_towerHour = edit_towerHour + change;
          edit_towerHour = checkRange(edit_towerHour, 0, 23);
          break;
        case 7:  // tower minute
                 // updating the tower clock minutes
          edit_towerMinute = edit_towerMinute + change;
          edit_towerMinute = checkRange(edit_towerMinute, 0, 59);
          break;
        case 8:  // compensate after days
          edit_compensateAfterDays = edit_compensateAfterDays + change;
          edit_compensateAfterDays = checkRange(edit_compensateAfterDays, 0, 365);
          break;
        case 9:  // compensate seconds
          edit_compensateSeconds = edit_compensateSeconds + change;
          edit_compensateSeconds = checkRange(edit_compensateSeconds, -1, 1);
          break;
      }
    }
    updateEditModeDisplay();
  } else {
    mainScreenIndex = mainScreenIndex + change;
    mainScreenIndex = checkRange(mainScreenIndex, 1, 2);
  }
}

// check changed value is inside the range
int checkRange(int value, int minValue, int maxValue) {
  if (value > maxValue) {
    value = minValue;
  } else if (value < minValue) {
    value = maxValue;
  }
  return value;
}

// start edit mode - initialize some variables
void enterEditMode() {
  editMode = true;
  currentPageIndex = 1;  // go to the first item to change
  selectedPageIndex = 0;
  // copy current values in edit ones
  edit_year = year;
  edit_month = month;
  edit_day = day;
  edit_hour = hour;
  edit_minute = minute;
  edit_towerHour = towerHour;
  edit_towerMinute = towerMinute;
  edit_compensateAfterDays = compensateAfterDays;
  edit_compensateSeconds = compensateSeconds;
}

// user exited the the edit mode - reinitialize some variables
void exitEditMode() {
  currentPageIndex = 1;
  selectedPageIndex = 0;
  editMode = false;
  mainScreenIndex = 1;
}

// get day of week names from index 1-Starting on Monday and 7 as Sunday
String getWeekDayName(uint8_t dayIndex) {
  switch (dayIndex) {
    case 1:
      return F("Mon");
      break;
    case 2:
      return F("Tue");
      break;
    case 3:
      return F("Wed");
      break;
    case 4:
      return F("Thu");
      break;
    case 5:
      return F("Fri");
      break;
    case 6:
      return F("Sat");
      break;
    case 7:
      return F("Sun");
      break;
    // Add more cases for other days
    default:
      return F("Invalid");
      break;
  }
}

// check there is setting for self adjustment for the time in RTC module and time already passed since last adjustment
void checkNeedToCompensateTime() {
  if ((compensateAfterDays > 0) && (compensateSeconds != 0) && (second == 30)) {  // compensate only in the middle of minute to prevent overflow to next minute, hour, day, month or even year
    if (secondsElapsedSinceLastCompensation > secondsToCompensate) {
      secondsElapsedSinceLastCompensation = 0;
      if (compensateSeconds > 0) {
        second++;
        totalSecondsCompensated++;
      } else {
        second--;
        totalSecondsCompensated--;
      }
      myRTC.adjust(DateTime(year, month, day, hour, minute, second));  //
      //lastTimeCompensated = currentTime;
    }
  }
}

// check if we need to update the time because of DST rules in Europe
// changing the time on last Sunday in March and October
void checkDaylightSavingChanges() {
  // check last Sunday in March or October and update the time accordingly
  // earliest possible date in 25. March or October and latest 31. March or October
  if (dayOfWeek == 7 && day >= 25) {
    if (month == 3 && hour == 2 && minute == 0) {
      // moving hour forward on 02:00 to 03:00
      hour = 3;
      myRTC.adjust(DateTime(year, month, day, hour, minute, second));
      isSummerTime = true;
    }
    // moving hour backwards from 03:00 to 2:00 - do this only once otherwise will be in the loop
    else if (month == 10 && hour == 3 && minute == 0 && isSummerTime) {
      hour = 2;
      myRTC.adjust(DateTime(year, month, day, hour, minute, second));
      isSummerTime = false;
    }
  }
}
