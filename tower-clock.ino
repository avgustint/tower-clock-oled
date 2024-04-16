/*
  Control church tower clock with arduino microcontroller using Real Time Clock (RTC) Module - always try to synchronize tower clock time with controller system time
  Pause tower clock if there is no grid power that can rotate the motor
  Taking into consideration also rules for Daylight Saving Time (DST) in Europe
  Using rotary encoder to setup all variables
  Display program settings and date time on LCD display with auto turn on/off backlight
  Using Lithium battery as a controller UPS
  Reading and recording min/max temperature from RTC module

  Avgustin Tomsic
  March 2024
*/
#include <Wire.h>
#include <DS3231.h>
#include <LiquidCrystal.h>
#include <Bounce2.h>

// rotary encoder pins
#define CLK_PIN 2  // rotary encoder clock pin
#define SW_PIN 3   // rotary encoder switch pin
#define DT_PIN 8   // rotary encoder data pin

// LCD display pins
#define RS_PIN 12
#define EN_PIN 11
#define D4_PIN 7
#define D5_PIN 6
#define D6_PIN 5
#define D7_PIN 4
#define BACKLIGHT_PIN 9

// other pins
#define POWER_CHECK_PIN 10  // pin for checking power supply
#define RELAY_1_PIN 14      // define pin to trigger the relay 1
#define RELAY_2_PIN 15      // define pin to trigger the relay 2

// RTC variables
bool century = false;
bool h12Flag;
bool pmFlag;

// system time data
int year = 24;
int month = 1;
int day = 1;
int hour = 0;
int minute = 0;
int second = 0;
int dayOfWeek = 1;
bool isSummerTime = false;  // are we using summer or winter time depending on DST rules

// tower time - the current position of tower clock indicators
int towerHour = 0;
int towerMinute = 0;

// temperature
int lastTemp = 0;
int minTemp = 999;
int maxTemp = -999;
String minTempDate = "";
String maxTempDate = "";

// power supply
String lastFailureDate = "";
unsigned long upTimeSeconds = 0;
unsigned long downTimeSeconds = 0;

// setup mode variables
bool confirmationResult = false;
int edit_year = 24;
int edit_month = 1;
int edit_day = 1;
int edit_hour = 0;
int edit_minute = 0;
int edit_dayOfWeek = 1;
int edit_towerHour = 0;
int edit_towerMinute = 0;
int edit_compensateAfterDays = 0;
int edit_compensateSeconds = 0;
bool confirmReset = false;

// RTC module self compensation
int compensateAfterDays = 0;
int compensateSeconds = 0;  // possible values -1s, 0 or +1s
unsigned long secondsElapsedSinceLastCompensation = 0;
unsigned long secondsToCompensate = 0;
String lastTimeCompensated = "Never";
int totalSecondsCompensated = 0;

// app
const int NUM_OF_PAGES = 16;
// String screenPages[NUM_OF_PAGES] = { "sysTime", "sysDate", "towerTime", "compensate", "temp", "minTemp", "maxTemp", "power", "lastFailure", "uptime", "downtime", "lastSetup", "lastReset", "lastCompensate","sinceLastCompensate","totalCompensated" };
int currentPageIndex = 0;                       // current selected page to show on LCD
int controllerMode = 0;                         // in with mode is currently the controller 0: normal operation, 1: setup or edit mode, 2: reset mode
int editStep = 1;                               // current step of configuration for each screen
String lastTimeUpdate = "";                     // storing last updated time on LCD - to not refresh LCD to often
String lastTimeSetup = "Unknown";               // storing when we updated the date and time last time - info purposes only to check how accurate is RTC module over time
String lastTimeStartup = "";                    // storing value since when the controller is running - this will be wrong time if RTC module never initialized before and is fresh one
unsigned long lastBacklightOpen = 0;            // stored time when LCD backlight was opened
unsigned int backlightDuration = 30000;         // the duration of LCD backlight turn on time in miliseconds (30s)
unsigned long lastEditModeChange = 0;           // stored time when user done some interaction in edit mode - auto cancel edit mode after timeout
unsigned int editModeAutoExitDuration = 60000;  // the duration of waiting time in edit mode after which we auto close edit mode without changes (60s)
bool motorRotating = false;                     // current state of the motor - true: rotating, false: motor not rotating any more
unsigned long motorOpenStart = 0;               // when motor started to rotate
int nextRelayState = 0;                         // next state of relay

// relay
int relayDelay = 5000;  // relay delay betwen next time changing state

// initialize objects
LiquidCrystal lcd(RS_PIN, EN_PIN, D4_PIN, D5_PIN, D6_PIN, D7_PIN);  // declare object for LCD display manipulation
DS3231 myRTC;                                                       // object for getting and setting time in Real Time Clock module

Bounce debouncerSwitch = Bounce();  // Create a Bounce object to prevent any glitches from rotary encoder switch
Bounce debouncerClk = Bounce();     // Create a Bounce object to prevent any glitches from rotary encoder clock signal

// initialize the controller
void setup() {
  Wire.begin();      // start I2C communication with RTC module
  lcd.begin(16, 2);  // set up the LCD's number of columns and rows:

  // setup controller pins
  pinMode(CLK_PIN, INPUT);
  pinMode(DT_PIN, INPUT);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT);
  pinMode(POWER_CHECK_PIN, INPUT);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  debouncerSwitch.attach(SW_PIN);      // Attach to the rotary encoder press pin
  debouncerSwitch.interval(2);         // Set debounce interval (in milliseconds)
  debouncerClk.attach(CLK_PIN);        // Attach to the rotary encoder clock pin
  debouncerClk.interval(2);            // Set debounce interval (in milliseconds)
  digitalWrite(RELAY_1_PIN, HIGH);     // turn on the relay 1
  digitalWrite(RELAY_2_PIN, LOW);      // turn off the relay 2
  nextRelayState = 0;                  // must be just vice vrsa what is currently set 
  getCurrentTime();                    // read current time from RTC module
  isSummerTime = checkIsSummerTime();  // check if currently is Summer or Winter time depending to DST rules
  // initialize tower clock on same as current time  - will be set correctly in edit mode - here just to prevent rotating the tower clock
  towerHour = hour;
  towerMinute = minute;
  showBacklight();  // turn the lcd backlight on
  lastTimeStartup = getFormatedDate(year, month, day) + " " + getFormatedShortTime(hour, minute);
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

  if (controllerMode == 1)  // edit mode
  {
    // check auto exit edit mode timeout already passed
    if (checkTimeoutExpired(lastEditModeChange, editModeAutoExitDuration)) {
      exitEditMode(false);  // auto exit edit mode after no interaction timeout
    }
  } else {
    getCurrentTime();             // read current time from RTC module
    updateScreenDateTime();       // check need to update the LCD screen
    updateTowerClock();           // check need to update tower clock
    checkNeedToCompensateTime();  // check any setting to self adjust time in RTC module
  }
  checkCloseMotorRelay();  // check rotating delay expired
  checkLcdBacklight();     // check need to turn off the LCD backlight
}

// reset function to have ability to reset Arduino programmatically
void (*resetFunc)(void) = 0;

// read the current time from the RTC module
void getCurrentTime() {
  year = myRTC.getYear();
  month = myRTC.getMonth(century);
  day = myRTC.getDate();
  hour = myRTC.getHour(h12Flag, pmFlag);
  minute = myRTC.getMinute();
  second = myRTC.getSecond();
  dayOfWeek = myRTC.getDoW();
  // check time need to be updated because of the DST rules
  checkDaylightSavingChanges();
}

// call the update lcd screen only when date time changes and we are showing the current time to prevent updating the LCD to often
void updateScreenDateTime() {
  // get the current date time
  String currentTime = getFormatedDate(year, month, day) + " " + getFormatedTime(hour, minute, second);
  if (lastTimeUpdate != currentTime) {
    secondsElapsedSinceLastCompensation++;
    if (isGridPowerOn()) {
      upTimeSeconds++;
    } else {
      downTimeSeconds++;
      lastFailureDate = getFormatedDate(year, month, day) + " " + getFormatedShortTime(hour, minute);
    }
    lastTimeUpdate = currentTime;
    // update the lcd only if date time changed and showing hour screen
    // if screen sysTime, uptime, downtime, sinceLastCompensate
    if ((currentPageIndex == 0 || currentPageIndex == 11 || currentPageIndex == 12 || currentPageIndex == 16) && !motorRotating) {
      updateDisplay();
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
      myRTC.setHour(hour);
      isSummerTime = true;
    }
    // moving hour backwards from 03:00 to 2:00 - do this only once otherwise will be in the loop
    else if (month == 10 && hour == 3 && minute == 0 && isSummerTime) {
      hour = 2;
      myRTC.setHour(hour);
      isSummerTime = false;
    }
  }
}

// update the tower clock if minute incremented
void updateTowerClock() {
  int currentDayMinutes = getSystemMinutes();             // using system time - get number of minutes in day for 12h format
  int towerDayMinutes = getTowerMinutes();                // using tower time - get number of minutes in day  of tower time for 12h format
  int minutesDiff = currentDayMinutes - towerDayMinutes;  // calculate minutes diff between controller time and tower time
  // minutesDiff > 0 curent time is ahead of tower time - try to catch up, but only if current time is less then 6 hours ahead, otherwise wait
  int sixHoursDiff = 6 * 60;
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
    incrementTowerClock();         // increment the tower time fist to display correct new time on lcd screen
    showOperationMessage();        // show message on lcd screen that motor is operation
    motorRotating = true;          // set variable that relay is open and motor is in rotating state
    motorOpenStart = millis();     // store time when we start rotating the motor to stop after timeout
    digitalWrite(RELAY_1_PIN, nextRelayState);  // internate both relay between  different states
    nextRelayState = nextRelayState==1 ? 0 : 1; // toggle the state
    digitalWrite(RELAY_2_PIN, nextRelayState);  // internate both relay between different states
  }
}

// check if we need to close the relay or motor stopped rotating
// calculate time difference instead of using the delays
// do not trigger the relay for next 5 seconds - to not rotate too quickly when hour change on DST also motor is rotating longer then relay is open - motor is having own relay to turn off when rotated enough
void checkCloseMotorRelay() {
  if (motorRotating && checkTimeoutExpired(motorOpenStart, relayDelay)) {
    motorRotating = false;
    updateDisplay();  // update the display with new tower time and removing operational message
  }
}

// on user interaction turn on LCD backlight and remember time light turned on so that we can turn off after timeout
void showBacklight() {
  lastBacklightOpen = millis();
  digitalWrite(BACKLIGHT_PIN, HIGH);
}

// check LCD backlight is in timeout and we need to turn off the backlight
void checkLcdBacklight() {
  if (checkTimeoutExpired(lastBacklightOpen, backlightDuration)) {
    digitalWrite(BACKLIGHT_PIN, LOW);
  }
}

// common function to check if different timeouts already expired from last change
bool checkTimeoutExpired(unsigned long lastChange, unsigned long timeoutDuration) {
  unsigned long currentMillis = millis();
  return ((currentMillis - lastChange) >= timeoutDuration);
}

// show message that motor in in the operation and rotating the clock indicators
void showOperationMessage() {
  // displayLcdMessage("Motor rotating..", "Tower time " + getFormatedShortTime(towerHour, towerMinute));
  displayLcdMessage("Rotating", "Time " + getFormatedShortTime(towerHour, towerMinute));
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

// function to check if we have power supply - reading pin value used to detect power supply
bool isGridPowerOn() {
  int powerOn = digitalRead(POWER_CHECK_PIN);
  return powerOn == HIGH ? true : false;
}

// event when user is rotating the rotary encoder button
void encoderRotated() {
  int change = 0;
  int dtState = digitalRead(DT_PIN);
  if (dtState == HIGH) {
    change = 1;  // rotated clockwise
  } else {
    change = -1;  // rotated anticlockwise
  }
  if (controllerMode == 1)  // edit mode
  {
    lastEditModeChange = millis();
    // we are in the edit mode - increment/decrement values accordingly depending on selected screen
    switch (currentPageIndex) {
      case 0:  // sysTime
        if (editStep == 1) {
          // update the system time hour
          edit_hour = edit_hour + change;
          edit_hour = checkRange(edit_hour, 0, 23);
        } else if (editStep == 2) {
          // update the system time minutes
          edit_minute = edit_minute + change;
          edit_minute = checkRange(edit_minute, 0, 59);
        }
        break;
      case 1:  // sysDate
        if (editStep == 1) {
          // update the system date day
          edit_day = edit_day + change;
          edit_day = checkRange(edit_day, 1, 31);
        } else if (editStep == 2) {
          // update the system time month
          edit_month = edit_month + change;
          edit_month = checkRange(edit_month, 1, 12);
        } else if (editStep == 3) {
          // update the system time year - changing only last 2 digits
          edit_year = edit_year + change;
          edit_year = checkRange(edit_year, 24, 99);  // minimum year can be current year 2024
        } else if (editStep == 4) {
          // changing the day of week
          edit_dayOfWeek = edit_dayOfWeek + change;
          edit_dayOfWeek = checkRange(edit_dayOfWeek, 1, 7);
        }
        break;
      case 2:  // towerTime
        if (editStep == 1) {
          // updating the tower clock hour
          edit_towerHour = edit_towerHour + change;
          edit_towerHour = checkRange(edit_towerHour, 0, 23);
        } else if (editStep == 2) {
          // updating the tower clock minutes
          edit_towerMinute = edit_towerMinute + change;
          edit_towerMinute = checkRange(edit_towerMinute, 0, 59);
        }
        break;
      case 3:  // compensate
        if (editStep == 1) {
          edit_compensateAfterDays = edit_compensateAfterDays + change;
          edit_compensateAfterDays = checkRange(edit_compensateAfterDays, 0, 365);
        } else if (editStep == 1){
          edit_compensateSeconds = edit_compensateSeconds + change;
          edit_compensateSeconds = checkRange(edit_compensateSeconds, -1, 1);
        }
        else {
          confirmationResult = !confirmationResult;
        }
        break;
      default:
        break;
    }
  } else if (controllerMode == 2) {
    confirmReset = !confirmReset;
  } else {
    changeScreen(change);  // when not in edit mode, just show next/previous screen
  }

  updateDisplay();  // update the lcd screen with latest values
  showBacklight();  // turn the lcd backlight on
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

// show next available screen depending the change direction
void changeScreen(int change) {
  currentPageIndex = currentPageIndex + change;
  if (currentPageIndex < 0) {
    currentPageIndex = NUM_OF_PAGES - 1;
  } else if (currentPageIndex >= NUM_OF_PAGES) {
    currentPageIndex = 0;
  }
}

// user presses encoder button
void encoderPressed() {
  showBacklight();  // turn on LCD backlight
  lastEditModeChange = millis();
  if (controllerMode == 1)  // edit mode
  {
    // in edit mode go to the next step of configuration
    editStep++;
    switch (currentPageIndex) {
      case 0:  // sysTime
        if (editStep == 3) {
          // when last step of setting time - go to the step of setting date
          currentPageIndex = currentPageIndex + 1;
          editStep = 1;  // moving to the date update
        }
        break;
      case 1:  // sysDate
        if (editStep == 5) {
          // when last step on setting date
          currentPageIndex = currentPageIndex + 1;
          editStep = 1;  // moving to the tower time setup
        }
        break;
      case 2:  // towerTime
        if (editStep == 3) {
          // on last step on setting tower time
          currentPageIndex = currentPageIndex + 1;
          editStep = 1;  // moving to the compensation setup
        }
        break;
      case 3:  // compensate
        if (editStep == 4) {
          if (confirmationResult) {
            // finishing the edit mode - update the RTC module with new settings
            myRTC.setHour(edit_hour);
            myRTC.setMinute(edit_minute);
            myRTC.setSecond(0);
            myRTC.setYear(edit_year);
            myRTC.setMonth(edit_month);
            myRTC.setDate(edit_day);
            myRTC.setDoW(edit_dayOfWeek);
            towerHour = edit_towerHour;
            towerMinute = edit_towerMinute;
            compensateAfterDays = edit_compensateAfterDays;
            compensateSeconds = edit_compensateSeconds;
            secondsElapsedSinceLastCompensation = 0;
            secondsToCompensate = compensateAfterDays * 86400;
            isSummerTime = checkIsSummerTime();
            lastTimeSetup = getFormatedDate(edit_year, edit_month, edit_day) + " " + getFormatedShortTime(edit_hour, edit_minute);
            exitEditMode(true);  // move to normal operational mode
          } else {
            exitEditMode(false);  // move to normal operational mode
          }
        }
        break;
      default:
        break;
    }
  } else if (controllerMode == 2) {
    if (confirmReset) {
      resetFunc();
    } else {
      controllerMode = 0;
      editStep = 1;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Cancelled!");
      delay(500);
    }
  } else {
    if (currentPageIndex == 14) { // lastReset
      controllerMode = 2;
      editStep = 1;
      confirmReset = false;
    } else {
      // currently not in edit mode and button pressed
      enterEditMode();
    }
  }
  updateDisplay();  // update the LCD display with new values
}

// start edit mode - initialize some variables
void enterEditMode() {
  controllerMode = 1;
  currentPageIndex = 0;  // go to the first screen to update all parameters
  editStep = 1;
  confirmationResult = false;
  // copy current values in edit ones
  edit_year = year;
  edit_month = month;
  edit_day = day;
  edit_hour = hour;
  edit_minute = minute;
  edit_dayOfWeek = dayOfWeek;
  edit_towerHour = towerHour;
  edit_towerMinute = towerMinute;
  edit_compensateAfterDays = compensateAfterDays;
  edit_compensateSeconds = compensateSeconds;
}

// user exited the the edit mode - reinitialize some variables
void exitEditMode(bool valuesUpdated) {
  currentPageIndex = 0;
  controllerMode = 0;
  editStep = 1;
  lcd.clear();
  lcd.setCursor(0, 0);
  valuesUpdated ? lcd.print("Values saved!") : lcd.print("Cancelled!");
  delay(500);
}

// update the LCD screen with all the appropriate data
void updateDisplay() {
  if (controllerMode == 1) {
    switch (currentPageIndex) {
      case 0:  // sysTime
        if (editStep == 1) {
          // configuring system time hour
          displayLcdMessage("Set System Hour", String(edit_hour));
        } else if (editStep == 2) {
          // configuring system time minutes
          displayLcdMessage("Set System Min.", String(edit_minute));
        }
        break;
      case 1:  // sysDate
        if (editStep == 1) {
          // configuring system time day
          displayLcdMessage("Set System Day", String(edit_day));
        } else if (editStep == 2) {
          // configuring system time month
          displayLcdMessage("Set System Month", String(edit_month));
        } else if (editStep == 3) {
          // configuring system time year
          displayLcdMessage("Set System Year", "20" + String(edit_year));
        } else if (editStep == 4) {
          // configuring day of week - Monday, Tuesday,...
          displayLcdMessage("Set Day Of Week", getWeekDayName(edit_dayOfWeek));
        }
        break;
      case 2:  // towerTime
        if (editStep == 1) {
          // configuring tower clock hour
          displayLcdMessage("Set Tower Hour", String(edit_towerHour));
        } else if (editStep == 2) {
          // configuring tower clock minutes
          displayLcdMessage("Set Tower Min.", String(edit_towerMinute));
        }
        break;
      case 3:  // compensate
        if (editStep == 1) {
          // configuring tower clock hour
          displayLcdMessage("Compensate Time", "After days: " + String(edit_compensateAfterDays));
        } else if (editStep == 2) {
          // configuring tower clock minutes
          displayLcdMessage("Compensate", "Seconds: " + String(edit_compensateSeconds));
        }
        else {
          displayLcdMessage("Confirm changes?", confirmationResult ? "Yes" : "No");
        }
        break;
      default:
        break;
    }
  } else if (controllerMode == 2) {
    displayLcdMessage("Reset arduino?", confirmReset ? "Yes" : "No");
  } else {
    // we are in normal mode operation
    switch (currentPageIndex) {
      case 0:  // sysTime
        // display current system time and day of week
        displayLcdMessage("System Time", getFormatedTime(hour, minute, second) + " " + getWeekDayName(dayOfWeek));
        break;
      case 1:  // sysDate
        // display current system date and Summer or Winter time
        displayLcdMessage("System Date", getFormatedDate(year, month, day) + " " + (isSummerTime ? "Summ." : "Wint."));
        break;
      case 2:  // towerTime
        // display current tower clock time
        displayLcdMessage("Tower Time", getFormatedShortTime(towerHour, towerMinute));
        break;
      case 3:  //compensate
        // display current tower clock time
        displayLcdMessage("Compensate after", String(compensateAfterDays) + " days " + String(compensateSeconds) + " sec");
        break;
      case 4:  // temp
        // display current temperature
        displayLcdMessage("Temperature", getTemp(lastTemp));
        break;
      case 5:  // minTemp
        // display minimum recorded temperature and date time that measured
        displayLcdMessage("Min. T. " + getTemp(minTemp), minTempDate);
        break;
      case 6:  // maxTemp
        // display maximum recorded temperature and date time that measured
        displayLcdMessage("Max. T. " + getTemp(maxTemp), maxTempDate);
        break;
      case 7:  // power
        // display if grid power supply is working
        displayLcdMessage("Power Supply", isGridPowerOn() ? "Ok" : "Fail");
        break;
      case 8:  // lastFailure
        // display power supply last failure date
        displayLcdMessage("Last Power Fail.", lastFailureDate != "" ? lastFailureDate : "Never");
        break;
      case 9:  // uptime
        // display power supply uptime seconds
        displayLcdMessage("Uptime Seconds", String(upTimeSeconds));
        break;
      case 10:  // downtime
        // display power supply down time seconds
        displayLcdMessage("Downtime Seconds", String(downTimeSeconds));
        break;
      case 11:  // lastSetup
        // display last time date and time has been adjusted with setup proces
        displayLcdMessage("Last Setup", lastTimeSetup);
        break;
      case 12:  // lastReset
        // display since when controller is running, this works well only if RTC has been adjusted before
        displayLcdMessage("Last Startup", lastTimeStartup);
        break;
      case 13:  // lastCompensate
        // display last date time that clock has been self adjusted
        displayLcdMessage("Last Compensate", lastTimeCompensated);
        break;
      case 14:  // sinceLastCompensate
        // display seconds elapsed since last time compensation
        displayLcdMessage("Last compen. sec", String(secondsElapsedSinceLastCompensation));
        break;
      case 15:  // totalCompensated
        // display total amount of seconds that were compensated
        displayLcdMessage("Total compensat.", String(totalSecondsCompensated));
        break;
      default:
        break;
    }
  }
}

// display 2 line message on LCD screen
void displayLcdMessage(String line1, String line2) {
  // clear the complete LCD screen
  lcd.clear();
  // move to first position on top row
  lcd.setCursor(0, 0);
  lcd.print(line1);
  // move to first position on bottom row
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

// format temperature with degrees symbol
String getTemp(int temp) {
  return String(temp) + (char)223 + "C";
}

// get day of week names from index 1-Starting on Monday and 7 as Sunday
String getWeekDayName(int dayIndex) {
  switch (dayIndex) {
    case 1:
      return "Mon";
      break;
    case 2:
      return "Tue";
      break;
    case 3:
      return "Wed";
      break;
    case 4:
      return "Thu";
      break;
    case 5:
      return "Fri";
      break;
    case 6:
      return "Sat";
      break;
    case 7:
      return "Sun";
      break;
    // Add more cases for other days
    default:
      return "Invalid";
      break;
  }
}

// add leading zero if required and semicolon
String printDigits(int digits, bool addSemicolumn) {
  // utility function for digital clock display: prints preceding colon and leading 0
  String output = "";
  if (addSemicolumn)
    output = ":";
  if (digits < 10)
    output = output + "0";
  output = output + String(digits);
  return output;
}

// get formatted input time with hours minutes and seconds
String getFormatedTime(int hour, int minute, int second) {
  return printDigits(hour, false) + printDigits(minute, true) + printDigits(second, true);
}

// get formatted input date with year, month, day
String getFormatedDate(int year, int month, int day) {
  return String(day) + "." + String(month) + ".20" + String(year);
}

// display only hours and minutes
String getFormatedShortTime(int hour, int minute) {
  return printDigits(hour, false) + printDigits(minute, true);
}

// read the current temperature from the RTC module - the precision of the temperature sensor is ± 3°C
void checkTemperature() {
  // read current temperature
  lastTemp = myRTC.getTemperature();
  // check current temperature exceeds max recorded temp
  if (lastTemp > maxTemp) {
    maxTemp = lastTemp;
    maxTempDate = getFormatedDate(year, month, day) + " " + getFormatedShortTime(hour, minute);
  }
  // check current temp is lower them min recorded temp
  if (lastTemp < minTemp) {
    minTemp = lastTemp;
    minTempDate = getFormatedDate(year, month, day) + " " + getFormatedShortTime(hour, minute);
  }
}

// get number of minutes in a day by using only 12h format - because on tower we have only 12 hours
int getSystemMinutes() {
  int currentHour = hour % 12;  // if hour is in 24h format is the same for tower clock to use 12 hour format
  return currentHour * 60 + minute;
}

// get number of minutes of tower clock in a day by using only 12h format
int getTowerMinutes() {
  int currentHour = towerHour % 12;  // if hour is in 24h format is the same for tower clock to use 12 hour format
  int dayMinutes = currentHour * 60 + towerMinute;
  if (dayMinutes == 719) {  // if last minute in a day then return -1, because first minute in a day using system time will be zero, otherwise tower clock will not be incremented
    dayMinutes = -1;
  }
  return dayMinutes;
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
      myRTC.setSecond(second);
      lastTimeCompensated = getFormatedDate(year, month, day) + " " + getFormatedShortTime(hour, minute);
    }
  }
}
