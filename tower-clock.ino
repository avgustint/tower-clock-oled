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
#define POWER_CHECK_PIN 10   // pin for checking power supply
#define RELAY_PIN 14         //define pin to trigger the relay


// variables
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

// app
const int NUM_OF_PAGES = 8;
String screenPages[NUM_OF_PAGES] = { "sysTime", "sysDate", "towerTime", "delay", "temp", "minTemp", "maxTemp", "power" };
int currentPageIndex = 0;             // current selected page to show on LCD
bool editMode = false;                // are we in edit mode flag
int editStep = 1;                     // current step of configuration for each screen
String lastTimeUpdate = "";           // storing last updated time on LCD - to not refresh LCD to often
unsigned long lastBacklightOpen = 0;  // stored time when LCD backlight was opened   
int backlightDuration = 30000;        // the duration of LCD backlight turn on time in miliseconds (30s)

// relay
int relayDelay = 1500;

// initialize objects
LiquidCrystal lcd(RS_PIN, EN_PIN, D4_PIN, D5_PIN, D6_PIN, D7_PIN); // declare object for LCD display manipulation
DS3231 myRTC;                                                      // object for getting and setting time in Real Time Clock module

Bounce debouncerSwitch = Bounce();  // Create a Bounce object to prevent any glitches from rotary encoder switch
Bounce debouncerClk = Bounce();     // Create a Bounce object to prevent any glitches from rotary encoder clock signal

// initialize the controller
void setup() {
  Wire.begin();                   // start I2C communication with RTC module
  lcd.begin(16, 2);               // set up the LCD's number of columns and rows:
  
  // setup controller pins
  pinMode(CLK_PIN, INPUT);
  pinMode(DT_PIN, INPUT);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(POWER_CHECK_PIN, INPUT);
  pinMode(BACKLIGHT_PIN,OUTPUT);
  debouncerSwitch.attach(SW_PIN);  // Attach to the rotary encoder press pin
  debouncerSwitch.interval(100);   // Set debounce interval (in milliseconds)
  debouncerClk.attach(CLK_PIN);    // Attach to the rotary encoder clock pin
  debouncerClk.interval(2);        // Set debounce interval (in milliseconds)
  digitalWrite(RELAY_PIN, HIGH);   // turn off the relay
  getCurrentTime();                // read current time from RTC module

  // initialize tower clock on same as current time  - will be set correctly in edit mode - here just to prevent rotating the tower clock
  towerHour = hour;
  towerMinute = minute;
  showBacklight();                 // turn the lcd backlight on
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

  if (!editMode) {
    getCurrentTime();         // read current time from RTC module 
    updateScreenDateTime();   // check need to update the LCD screen
    updateTowerClock();       // check need to update tower clock
  }
  checkLcdBacklight();        // check need to turn off the LCD backlight
}

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
void updateScreenDateTime(){
  // get the current date time
  String currentTime = getFormatedDate(year, month, day) + " " + getFormatedTime(hour, minute, second);
  String currentPage = screenPages[currentPageIndex];
  // update the lcd only if date time changed and showing hour screen
  if (lastTimeUpdate != currentTime && currentPage == "sysTime") {
    lastTimeUpdate = currentTime;
    updateDisplay();
  }
}

// check if we need to update the time because of DST rules in Europe 
// changing the time on last Sunday in March and October
void checkDaylightSavingChanges(){
  // check last Sunday in March or October and update the time accordingly
  // earliest possible date in 25. March or October and latest 31. March or October
  if (dayOfWeek==7 && day>=25){
    if (month==3 && hour==2 && minute==0){
      // moving hour forward on 02:00 to 03:00
      hour=3;
      myRTC.setHour(hour);
      isSummerTime = true;
    }
    // moving hour backwards from 03:00 to 2:00 - do this only once otherwise will be in the loop
    else if (month==10 && hour==3 && minute==0 && isSummerTime){
      hour=2;
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
    turnTheClock();       // need to turn the tower clock
    checkTemperature();   // every minute read also RTC module temperature
  } 
}

// Turn the tower clock if there is power supply from grid, because otherwise motor will not rotate when relay is triggered
bool turnTheClock() {
  // check we have power supply and not running on battery - when there is no power we can not run the motor
  if (isGridPowerOn()) {
    showOperationMessage();           // show message on lcd screen that motor is operation
    digitalWrite(RELAY_PIN, LOW);     // open relay
    delay(relayDelay);                // keep relay open for the delay setup in settings
    digitalWrite(RELAY_PIN, HIGH);    // close relay
    incrementTowerClock();            // increment the tower time
    delay(5000);                      // do not trigger the relay for next 5 seconds - to not rotate too quickly when hour change on DST also motor is rotating longer then relay is open - motor is having own relay to turn off when rotated enough 
    updateDisplay();                  // update the display with new tower time and removing operational message
    return true;
  }
  return false;
}

// on user interaction turn on LCD backlight and remenber time light turned on so that we can turn off after timeout
void showBacklight(){
    lastBacklightOpen = millis();
    digitalWrite(BACKLIGHT_PIN, HIGH);
}

// check LCD backlight is in timeout and we need to turn off the backlight
void checkLcdBacklight(){
  unsigned long currentMillis = millis();
  if (currentMillis - lastBacklightOpen >= backlightDuration) {// close the backlight after 30 seconds
    digitalWrite(BACKLIGHT_PIN,LOW);
  }
}

// show message that motor in in the operation and rotating the clock indicators
void showOperationMessage(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Motor rotating..");
  lcd.setCursor(0, 1);
  lcd.print("Please wait");
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
    change = 1;   //rotated clockwise
  } else {
    change = -1;  // rotated anticlockwise
  }
  if (editMode) {
    // we are in the edit mode - increment/decrement values accordingly depending on selected screen
    String currentPage = screenPages[currentPageIndex];
    if (currentPage == "sysTime") {
      if (editStep == 1) {
        // update the system time hour
        hour = hour + change;
        if (hour > 23) {
          hour = 0;
        } else if (hour < 0) {
          hour = 23;
        }
      } else if (editStep == 2) {
        // update the system time minutes
        minute = minute + change;
        if (minute > 59) {
          minute = 0;
        } else if (minute < 0) {
          minute = 59;
        }
      }
    } else if (currentPage == "sysDate") {
      if (editStep == 1) {
        // update the system date day
        day = day + change;
        if (day > 31) {
          day = 0;
        } else if (day < 1) {
          day = 31;
        }
      } else if (editStep == 2) {
        // update the system time month
        month = month + change;
        if (month > 12) {
          month = 1;
        } else if (month < 1) {
          month = 12;
        }
      } else if (editStep == 3) {
        // update the system time year - changing only last 2 digits
        year = year + change;
        if (year > 99) {
          year = 0;
        } else if (year < 0) {
          year = 99;
        }
      } else if (editStep == 4) {
        // changing the day of week
        dayOfWeek = dayOfWeek + change;
        if (dayOfWeek > 7) {
          dayOfWeek = 1;
        } else if (dayOfWeek < 1) {
          dayOfWeek = 7;
        }
      }
      else if (editStep == 5) {
        // changing the summer/winter time setting
        isSummerTime = !isSummerTime;
      }
    } else if (currentPage == "towerTime") {
      if (editStep == 1) {
        // updating the tower clock hour
        towerHour = towerHour + change;
        if (towerHour > 23) {
          towerHour = 0;
        } else if (towerHour < 0) {
          towerHour = 23;
        }
      } else if (editStep == 2) {
        // updating the tower clock minutes
        towerMinute = towerMinute + change;
        if (towerMinute > 59) {
          towerMinute = 0;
        } else if (towerMinute < 0) {
          towerMinute = 59;
        }
      }
    } else if (currentPage == "delay") {
      // updating the relay open state duration
      relayDelay = relayDelay + (change*10);
      if (delay > 4999) {
        relayDelay = 0;
      } else if (relayDelay < 0) {
        relayDelay = 4999;
      }
    }
  } else {
    changeScreen(change);     // when not in edit mode, just show next/previous screen
  }
  
  updateDisplay();            // update the lcd screen with latest values
  showBacklight();            // turn the lcd backlight on
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
  showBacklight(); // turn on LCD backlight 
  String currentPage = screenPages[currentPageIndex];
  if (editMode) {
    // in edit mode go to the next step of configuration
    editStep++;
    if (currentPage == "sysTime") {
      if (editStep == 3) {
        // when last step of setting time - go to the step of setting date 
        currentPageIndex = currentPageIndex+1;
        editStep = 1; // moving to the date update
      }
    } else if (currentPage == "sysDate") {
      if (editStep == 6) {
        // when last step on setting date 
        currentPageIndex = currentPageIndex+1;
        editStep = 1; // moving to the tower time setup
      }
    } else if (currentPage == "towerTime") {
      if (editStep == 3) {
        // on last step on setting tower time
        currentPageIndex = currentPageIndex+1;
        editStep = 1; // moving to the delay setup
      }
    } else if (currentPage == "delay") {
      // finishing the edit mode - update the RTC module with new settings
      myRTC.setHour(hour);
      myRTC.setMinute(minute);
      myRTC.setSecond(0);
      myRTC.setYear(year);
      myRTC.setMonth(month);
      myRTC.setDate(day);
      myRTC.setDoW(dayOfWeek);
      exitEditMode(); //move to normal operational mode
    }
  } else {
    // currently not in edit mode and button pressed - if we are on any of configurational screens the start edit mode procedure
    if (currentPage == "sysTime" || currentPage == "sysDate" || currentPage == "towerTime" || currentPage == "delay") {
      editMode = true;
      currentPageIndex = 0; // go to the first screen to update all parameters
      editStep = 1;
    }
  }
  updateDisplay();         // update the LCD display with new values
}

// user exited the the edit mode - reinitialize some variables
void exitEditMode() {
  editMode = false;
  editStep = 1;
}

// update the LCD screen with all the appropriate data
void updateDisplay() {
  // clear the complete LCD screen
  lcd.clear();
  // move to first position on top row 
  lcd.setCursor(0, 0);
  String currentPage = screenPages[currentPageIndex];
  if (editMode == true) {
    // we are in the edit mode
    if (currentPage == "sysTime") {
      if (editStep == 1) {
        // configuring system time hour
        lcd.print("Set System Hour");
        lcd.setCursor(0, 1);
        lcd.print(hour);
      } else if (editStep == 2) {
        // configuring system time minutes
        lcd.print("Set System Min.");
        lcd.setCursor(0, 1);
        lcd.print(minute);
      }
    } else if (currentPage == "sysDate") {
      if (editStep == 1) {
        // configuring system time day
        lcd.print("Set System Day");
        lcd.setCursor(0, 1);
        lcd.print(day);
      } else if (editStep == 2) {
        // configuring system time month
        lcd.print("Set System Month");
        lcd.setCursor(0, 1);
        lcd.print(month);
      } else if (editStep == 3) {
        // configuring system time year
        lcd.print("Set System Year");
        lcd.setCursor(0, 1);
        lcd.print(year);
      } else if (editStep == 4) {
        // configuring day of week - Monday, Tuesday,...
        lcd.print("Set Day Of Week");
        lcd.setCursor(0, 1);
        lcd.print(getWeekDayName());
      } else if (editStep == 5) {
        // configuring if we are currently using summer time or winter time
        lcd.print("Is Summer Time");
        lcd.setCursor(0, 1);
        lcd.print(isSummerTime?"Yes":"No");
      }
    } else if (currentPage == "towerTime") {
      if (editStep == 1) {
        // configuring tower clock hour
        lcd.print("Set Tower Hour");
        lcd.setCursor(0, 1);
        lcd.print(towerHour);
      } else if (editStep == 2) {
        // configuring tower clock minutes
        lcd.print("Set Tower Min.");
        lcd.setCursor(0, 1);
        lcd.print(towerMinute);
      }
    } else if (currentPage == "delay") {
      // configuring relay open state delay
      lcd.print("Set Relay delay");
      lcd.setCursor(0, 1);
      lcd.print(relayDelay);
    }
  } else {
    // we are in normal mode operation
    if (currentPage == "sysTime") {
      // display current system time and day of week
      lcd.print("System Time");
      lcd.setCursor(0, 1);
      lcd.print(getFormatedTime(hour, minute, second)+" "+getWeekDayName());
    } else if (currentPage == "sysDate") {
      // display current system date and Summer or Winter time
      lcd.print("System Date");
      lcd.setCursor(0, 1);
      lcd.print(getFormatedDate(year, month, day)+" "+(isSummerTime?"Summ.":"Wint."));
    } else if (currentPage == "towerTime") {
      // display current tower clock time
      lcd.print("Tower time");
      lcd.setCursor(0, 1);
      lcd.print(getFormatedShortTime(towerHour, towerMinute));
    } else if (currentPage == "temp") {
      // display current temperature
      lcd.print("Temperature");
      lcd.setCursor(0, 1);
      lcd.print(getTemp(lastTemp));
    } else if (currentPage == "minTemp") {
      // display minimum recorded temperature and date time that measured
      lcd.print("Min. T. " + getTemp(minTemp));
      lcd.setCursor(0, 1);
      lcd.print(minTempDate);
    } else if (currentPage == "maxTemp") {
      // display maximum recorded temperature and date time that measured
      lcd.print("Max. T. " + getTemp(maxTemp));
      lcd.setCursor(0, 1);
      lcd.print(maxTempDate);
    } else if (currentPage == "delay") {
      // display used relay open state delay duration in miliseconds
      lcd.print("Relay Delay [ms]");
      lcd.setCursor(0, 1);
      lcd.print(relayDelay);
    } else if (currentPage == "power") {
      // display if grid power supply is working  
      lcd.print("Power Supply");
      lcd.setCursor(0, 1);
      lcd.print(isGridPowerOn() ? "Ok" : "Fail");
    }
  }
}

// format temperature with degrees symbol
String getTemp(int temp){
  return String(temp)+(char)223+"C";
}

// get day of week names from index 1-Starting on Monday and 7 as Sunday
String getWeekDayName(){
  switch (dayOfWeek) {
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

// read the current temperature from the RTC module
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
  if (dayMinutes==719){ // if last minute in a day then return -1, because first minute in a day using system time will be zero, otherwise tower clock will not be incremented
    dayMinutes = -1;
  }
  return dayMinutes;
}
