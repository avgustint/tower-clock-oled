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
#define SCREEN_ADDRESS 0x3C  // I2C device address

// other pins
#define POWER_CHECK_PIN 10  // pin for checking power supply
#define SWITCH_1_PIN 14     // define pin to trigger the Solid State Relay SSR
#define SWITCH_2_PIN 15     // define pin to read motor encoder state

#define NO_ACTIVITY_DURATION 60000  // the duration of waiting time in edit mode after which we auto close edit mode without changes or turning display into sleep

// system time data
uint16_t year;
uint8_t month;
uint8_t day;
uint8_t hour;
uint8_t minute;
uint8_t second;
uint8_t dayOfWeek;
bool isSummerTime = false;  // are we using summer or winter time depending on DST rules

// tower time - the current position of tower clock indicators
uint8_t towerHour = 0;
uint8_t towerMinute = 0;

uint32_t currentSeconds;                     // 32-bit times as seconds since 2000-01-01.
uint32_t lastUpdateSeconds;                  // 32-bit times as seconds since 2000-01-01.
unsigned long lastUpdateEditModeScreen = 0;  // when we last updated screen in edit mode for blinking
DateTime currentTime;                        // list read time from RTC
bool editMode = false;                       // are we in edit mode
bool editModeBlinkDark = false;              // toggle blinking in edit mode when value selected
unsigned long lastUserInteraction = 0;       // stored time when user done some interaction in edit mode - auto cancel edit mode after timeout

// motor
bool motorRotating = false;  // current state of the motor - true: rotating, false: motor not rotating any more

// setup mode variables
uint8_t currentPageIndex;
// Possible values:
// 1 - day
// 2 - month
// 3 - year
// 4 - hour
// 5 - minute
// 6 - tower hour
// 7 - tower minute
// 8 - reset arduino
// 9 - confirmation setings
// 10 - cancel settings
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

// initialize objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);  // initialize OLED display
RTC_DS3231 myRTC;                                                          // initialize RTC module
Bounce debouncerSwitch = Bounce();                                         // Create a Bounce object to prevent any glitches from rotary encoder switch
Bounce debouncerClk = Bounce();                                            // Create a Bounce object to prevent any glitches from rotary encoder clock signal
Bounce debouncerMotor = Bounce();                                          // Create a Bounce object to prevent any glitches from motor encoder clock signal

// initialize the controller
void setup() {
  // start communication with RTC module
  if (!myRTC.begin()) {
    while (1)
      ;  // Don't proceed, loop forever
  }

  // start communication with OLED screen
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    while (1)
      ;  // Don't proceed, loop forever
  }
  // setup controller pins
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN, INPUT_PULLUP);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(POWER_CHECK_PIN, INPUT);
  pinMode(SWITCH_1_PIN, OUTPUT);
  pinMode(SWITCH_2_PIN, INPUT_PULLUP);
  digitalWrite(SWITCH_1_PIN, LOW);      // turn off the relay
  debouncerSwitch.attach(SW_PIN);       // Attach  debounce to the rotary encoder press pin
  debouncerSwitch.interval(2);          // Set debounce interval (in milliseconds)
  debouncerClk.attach(CLK_PIN);         // Attach to the rotary encoder clock pin
  debouncerClk.interval(2);             // Set debounce interval (in milliseconds)
  debouncerMotor.attach(SWITCH_2_PIN);  // Attach to the motor encoder clock pin
  debouncerMotor.interval(20);          // Set debounce interval (in milliseconds)
  getCurrentTime();                     // read current time from RTC module
  isSummerTime = checkIsSummerTime();   // check if currently is Summer or Winter time depending to DST rules
  // initialize tower clock on same as current time  - will be set correctly in edit mode - here just to prevent rotating the tower clock
  towerHour = hour;
  towerMinute = minute;
  // clear and initialize the display
  display.clearDisplay();
  display.display();  // this command will display all the data which is in buffer
  display.setTextWrap(false);
}

// main microcontroller loop
void loop() {
  // Update the debouncer
  debouncerSwitch.update();
  debouncerClk.update();
  debouncerMotor.update();

  // Read debounced rotary encoder press button state
  if (debouncerSwitch.fell()) {
    encoderPressed();
  }

  // Read debounced rotary encoder rotating clock event
  if (debouncerClk.fell()) {
    encoderRotated();
  }

  // event when motor encoder switch triggered
  if (debouncerMotor.changed()) {
    stopMotor();
  }

  getCurrentTime();  // read current time from RTC module
  if (editMode) {
    // edit mode
    if (checkTimeoutExpired(lastUpdateEditModeScreen, 300)) {  // update the screen every 300 miliseconds to show that blinking animation on selected value
      editModeBlinkDark = !editModeBlinkDark;                  // toggle between visible or hidden selected value
      lastUpdateEditModeScreen = millis();                     // store last time we toggled the stare for next itteration
      updateEditModeDisplay();                                 // update screen in edit mode
    }
    // check auto exit edit mode timeout already passed
    if (checkTimeoutExpired(lastUserInteraction, NO_ACTIVITY_DURATION)) {
      exitEditMode();  // auto exit edit mode after no interaction timeout
    }
  } else {
    // normal mode
    if (lastUpdateSeconds != currentSeconds) {
      // update the screen only once every second to prevent flickering and do other logic only once every second
      lastUpdateSeconds = currentSeconds;
      updateTowerClock();
      updateMainScreen();
    }
  }
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

  // check time need to be updated because of the DST rules
  checkDaylightSavingChanges();
}

// update the tower clock if minute incremented
void updateTowerClock() {
  uint16_t currentDayMinutes = getSystemMinutes();             // using system time - get number of minutes in day for 12h format
  uint16_t towerDayMinutes = getTowerMinutes();                // using tower time - get number of minutes in day  of tower time for 12h format
  uint16_t minutesDiff = currentDayMinutes - towerDayMinutes;  // calculate minutes diff between controller time and tower time
  // minutesDiff > 0 curent time is ahead of tower time - try to catch up, but only if current time is less then 10 hours ahead, otherwise wait whatever it takes
  uint16_t tenHoursDiff = 10 * 60;
  if (minutesDiff > 0 && minutesDiff < tenHoursDiff) {
    turnTheClock();  // need to turn the tower clock
  }
}

// Turn the tower clock, if there is power supply from grid, because otherwise motor will not rotate when relay is triggered
void turnTheClock() {
  // check we have power supply and not running on battery - when there is no power we can not run the motor
  // check also not already in motor rotating state
  if (motorRotating == false && isGridPowerOn()) {
    incrementTowerClock();             // increment the tower time first to display correct new time on OLED screen
    motorRotating = true;              // set variable that relay is open and motor is in rotating state
    digitalWrite(SWITCH_1_PIN, HIGH);  // open the relay to start rotating the motor
  }
}

// stop the motor rotation
void stopMotor() {
  // close the SSR relay to cancel the motor rotation
  digitalWrite(SWITCH_1_PIN, LOW);
  if (motorRotating && hour == 0 && minute == 0) {  // after updating the hour reset arduino at midnight
    resetFunc();
  }
  motorRotating = false;
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

// common function to check if different timeouts already expired from last change
bool checkTimeoutExpired(unsigned long lastChange, unsigned int timeoutDuration) {
  unsigned long currentMillis = millis();
  return ((currentMillis - lastChange) >= timeoutDuration);
}

// get number of minutes in a day by using only 12h format - because on tower we have only 12 hours
uint16_t getSystemMinutes() {
  uint16_t currentHour = hour % 12;  // if hour is in 24h format is the same for tower clock to use 12 hour format
  return currentHour * 60 + minute;
}

// get number of minutes of tower clock in a day by using only 12h format
uint16_t getTowerMinutes() {
  uint16_t currentHour = towerHour % 12;  // if hour is in 24h format is the same for tower clock to use 12 hour format
  uint16_t dayMinutes = currentHour * 60 + towerMinute;
  if (dayMinutes == 719) {  // if last minute in a day then return -1, because first minute in a day using system time will be zero, otherwise tower clock will not be incremented
    dayMinutes = -1;
  }
  return dayMinutes;
}

// display main screen info in normal operation - switch between different screens
void updateMainScreen() {
  display.fillRect(0, 0, 128, 64, SSD1306_BLACK);
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // show main screen with all valuable information
  addStatusInfo();
  display.setTextSize(1);
  display.setCursor(0, 22);
  display.setTextColor(WHITE);
  char currentDate[16];
  sprintf(currentDate, "%02d.%02d.%4d", day, month, year);  // add leading zeros to the day and month
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
    char rotatingText[10] = "Rotating";
    display.setCursor(50, 56);
    display.print(rotatingText);
  }
  display.display();
}

// display status bar information
void addStatusInfo() {
  display.setTextSize(1);
  // display power status
  display.setCursor(0, 0);
  char powerText[12];
  sprintf(powerText, "%s", isGridPowerOn() ? "Power: OK" : "Power: Fail");
  display.print(powerText);
  // display Summer/Winter Time
  display.setCursor(0, 10);
  char seasonText[8];
  sprintf(seasonText, "%s", isSummerTime ? "Summer" : "Winter");
  display.print(seasonText);
}

// show screen in edit mode
void updateEditModeDisplay() {
  display.fillRect(0, 0, 128, 64, SSD1306_BLACK);
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  char systemText[8] = "System:";
  display.print(systemText);
  display.setCursor(68, 0);
  char editDate[16];
  sprintf(editDate, "%02d.%02d.%4d", edit_day, edit_month, edit_year);  // add leading zeros to the day and month
  display.print(editDate);
  display.setCursor(68, 12);
  char editTime[16];
  sprintf(editTime, "%02d:%02d:%02d ", edit_hour, edit_minute, 0);
  display.print(editTime);
  display.setCursor(0, 24);
  char towerText[8] = "Tower:";
  display.print(towerText);
  display.setCursor(68, 24);
  char towerTime[16];
  sprintf(towerTime, "%02d:%02d ", edit_towerHour, edit_towerMinute);
  display.print(towerTime);
  display.setCursor(0, 54);
  char resetText[6] = "Reset";
  display.print(resetText);
  display.setCursor(40, 54);
  char confirmText[8] = "Confirm";
  display.print(confirmText);
  display.setCursor(90, 54);
  char cancelText[8] = "Cancel";
  display.print(cancelText);
  // no item selected to edit  - draw line under option
  if (currentPageIndex != selectedPageIndex) {
    // 1 - day
    // 2 - month
    // 3 - year
    // 4 - hour
    // 5 - minute
    // 6 - tower hour
    // 7 - tower minute
    // 8 - reset arduino
    // 9 - confirmation setings
    // 10 - cancel settings
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
      case 8:  // reset
        display.drawFastHLine(0, 63, 30, WHITE);
        break;
      case 9:  // confirm
        display.drawFastHLine(40, 63, 41, WHITE);
        break;
      case 10:  // cancel
        display.drawFastHLine(90, 63, 35, WHITE);
        break;
    }
  } else {
    // one of options selected to edit - blink that option
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

// function to check if we have power supply - reading pin value used to detect power supply
bool isGridPowerOn() {
  return digitalRead(POWER_CHECK_PIN);
}

// calculate current date is using Summer or Winter DST rules
bool checkIsSummerTime() {
  // earliest possible date
  // 25 26 27 28 29 30 31 - date
  // 0  1  2  3  4  5  6  - day of week index

  // latest possible date
  // 25 26 27 28 29 30 31
  // 1  2  3  4  5  6  0

  if (month < 3 || month > 10)
    return false;
  if (month > 3 && month < 10)
    return true;

  // calculate previous sunday day
  uint8_t previousSunday = day - dayOfWeek % 7;  // use different indexes the app 0 - Sunday, 1-Monday,..., 6-Saturday

  if (month == 3) {
    if (dayOfWeek > 0) {
      return previousSunday >= 25;
    } else {  // if is Sunday
      return previousSunday >= 25 && hour >= 3;
    }
  }

  if (month == 10) {
    if (dayOfWeek > 0) {
      return previousSunday < 25;
    } else {  // if Sunday
      return previousSunday < 25 || hour < 2;
    }
  }


  return false;  // this line never gonna happened
}

// user presses encoder button
void encoderPressed() {
  lastUserInteraction = millis();
  if (editMode) {
    if (currentPageIndex == 8) {
      // reset
      resetFunc();
    } else if (currentPageIndex == 9) {
      // confirm
      // finishing the edit mode - update the RTC module with new settings
      myRTC.adjust(DateTime(edit_year, edit_month, edit_day, edit_hour, edit_minute, 0));
      towerHour = edit_towerHour;
      towerMinute = edit_towerMinute;
      isSummerTime = checkIsSummerTime();
      exitEditMode();  // move to normal operational mode
    } else if (currentPageIndex == 10) {
      // cancel
      exitEditMode();
    } else {
      if (currentPageIndex != selectedPageIndex) {
        // select currently underlined option
        selectedPageIndex = currentPageIndex;
      } else {
        // deselect currently selected option
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
  lastUserInteraction = millis();
  uint8_t change = 0;

  int dtState = digitalRead(DT_PIN);
  if (dtState == HIGH) {
    change = 1;  // rotated clockwise
  } else {
    change = -1;  // rotated anticlockwise
  }
  if (editMode) {  // edit mode
    if (currentPageIndex != selectedPageIndex) {
      currentPageIndex = currentPageIndex + change;
      currentPageIndex = checkRange(currentPageIndex, 1, 10);
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
      }
    }
    updateEditModeDisplay();
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
}

// user exited the the edit mode - reinitialize some variables
void exitEditMode() {
  currentPageIndex = 1;
  selectedPageIndex = 0;
  editMode = false;
}

// get day of week names from index 1-Starting on Monday and 7 as Sunday
String getWeekDayName(uint8_t dayIndex) {
  switch (dayIndex) {
    case 0:
      return F("Sun");
      break;
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
    // Add more cases for other days
    default:
      return F("Invalid");
      break;
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
