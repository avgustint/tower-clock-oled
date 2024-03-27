#include <Wire.h>
#include <DS3231.h>
#include <LiquidCrystal.h>
#include <Bounce2.h>

// rotary encoder pins
#define CLK_PIN 2  // rotary encoder clock pin
#define SW_PIN 3   // rotary encoder switch pin
#define DT_PIN 8   // rotary endoder data pin

// LCD display pins
#define RS_PIN 12
#define EN_PIN 11
#define D4_PIN 7
#define D5_PIN 6
#define D6_PIN 5
#define D7_PIN 4
#define BACKLIGHT_PIN 0

#define POWER_CHECK_PIN 10  // pin for checking power supply
#define RELAY_PIN 9       //define pin to trigger the relay


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

// tower time
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
int currentPageIndex = 0;
bool editMode = false;
int editStep = 1;
String lastTimeUpdate = "";
unsigned long lastBacklightOpen = 0;
bool isSummerTime = false;

// relay
int relayDelay = 1500;

// initialize objects
LiquidCrystal lcd(RS_PIN, EN_PIN, D4_PIN, D5_PIN, D6_PIN, D7_PIN);
DS3231 myRTC;

Bounce debouncerSwitch = Bounce();  // Create a Bounce object
Bounce debouncerClk = Bounce();     // Create a Bounce object

void setup() {
  Wire.begin();
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  pinMode(CLK_PIN, INPUT);
  pinMode(DT_PIN, INPUT);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(POWER_CHECK_PIN, INPUT);
  pinMode(BACKLIGHT_PIN,OUTPUT);
  debouncerSwitch.attach(SW_PIN);  // Attach the button pin
  debouncerSwitch.interval(100);   // Set debounce interval (in milliseconds)
  debouncerClk.attach(CLK_PIN);    // Attach the button pin
  debouncerClk.interval(2);        // Set debounce interval (in milliseconds)
  digitalWrite(RELAY_PIN, HIGH);
  delay(500);
  // myRTC.setClockMode(false);  // set to 12h
  // myRTC.setYear(24);
  // myRTC.setMonth(3);
  // myRTC.setDate(25);
  // myRTC.setDoW(1);
  // myRTC.setHour(17);
  // myRTC.setMinute(33);
  // myRTC.setSecond(0);
  getCurrentTime();
  towerHour = hour;
  towerMinute = minute;
  // Print a message to the LCD.
  lcd.print("Initializing...");
}

void loop() {
  debouncerSwitch.update();  // Update the debouncer
  debouncerClk.update();

  // Read debounced button state
  if (debouncerSwitch.fell()) {
    encoderPressed();
  }

  if (debouncerClk.fell()) {
    encoderRotated();
  }

  if (!editMode) {
    getCurrentTime();

    String currentTime = getFormatedDate(year, month, day) + " " + getFormatedTime(hour, minute, second);
    String currentPage = screenPages[currentPageIndex];
    if (lastTimeUpdate != currentTime && currentPage == "sysTime") {
      lastTimeUpdate = currentTime;
      updateDisplay();
    }
    updateTowerClock();
  }
  unsigned long currentMillis = millis();
  if (currentMillis - lastBacklightOpen >= 30000) {// close the backlight after 30 seconds
    digitalWrite(BACKLIGHT_PIN,LOW);
  }

}

void getCurrentTime() {
  year = myRTC.getYear();
  month = myRTC.getMonth(century);
  day = myRTC.getDate();
  hour = myRTC.getHour(h12Flag, pmFlag);
  minute = myRTC.getMinute();
  second = myRTC.getSecond();
  dayOfWeek = myRTC.getDoW();
  checkDaylightSavingChanges();
}

void checkDaylightSavingChanges(){
  // check last Sunday in March or October and update the time accordingly
  // earliest possible date in 25. MArch or October and latest 31. March or October
  if (dayOfWeek==7 && day>=25 && (month==3 || month=10)){
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

void updateTowerClock() {
  checkTemperature();
  int currentDayMinutes = getSystemMinutes();  // using system time - get number of minutes in day for 12h format
  int towerDayMinutes = getTowerMinutes();     // using tower time - get number of minutes in day  of tower time for 12h format
  // calculate minutes diff between controller time and tower time
  int minutesDiff = currentDayMinutes - towerDayMinutes;
  // minutesDiff > 0 curent time is ahead of tower time - try to catch up, but only if current time is less then 6 hours ahead, otherwise wait
  int sixHoursDiff = 6 * 60;
  if (minutesDiff > 0 && minutesDiff < sixHoursDiff) {
    // need to turn the clock
    turnTheClock();
  } 
}

// try to turn the clock on tower
bool turnTheClock() {
  // check we have power supply and not running on battery - when there is no power we can not run the motor
  if (isGridPowerOn()) {
    showOperationMessage();
    // open relay
    digitalWrite(RELAY_PIN, LOW);
    delay(relayDelay);
    // close relay
    digitalWrite(RELAY_PIN, HIGH);
    // increment the tower time
    incrementTowerClock();
    delay(5000);  // do not trigger the relay for next 5 seconds - to not rotate too quickly on hour change
    updateDisplay();
    return true;
  }
  return false;
}

void showBacklight(){
    lastBacklightOpen = millis();
    digitalWrite(BACKLIGHT_PIN, HIGH);
}

void showOperationMessage(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Operating...");
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

void encoderRotated() {
  int change = 0;
  int dtState = digitalRead(DT_PIN);
  if (dtState == HIGH) {
    change = 1;  //rotated clockwise
  } else {
    change = -1;  // rotated anticlockwise
  }
  if (editMode) {
    String currentPage = screenPages[currentPageIndex];
    if (currentPage == "sysTime") {
      if (editStep == 1) {
        hour = hour + change;
        if (hour > 23) {
          hour = 0;
        } else if (hour < 0) {
          hour = 23;
        }
      } else if (editStep == 2) {
        minute = minute + change;
        if (minute > 59) {
          minute = 0;
        } else if (minute < 0) {
          minute = 59;
        }
      }
    } else if (currentPage == "sysDate") {
      if (editStep == 1) {
        day = day + change;
        if (day > 31) {
          day = 0;
        } else if (day < 1) {
          day = 31;
        }
      } else if (editStep == 2) {
        month = month + change;
        if (month > 12) {
          month = 1;
        } else if (month < 1) {
          month = 12;
        }
      } else if (editStep == 3) {
        year = year + change;
        if (year > 99) {
          year = 0;
        } else if (year < 0) {
          year = 99;
        }
      } else if (editStep == 4) {
        dayOfWeek = dayOfWeek + change;
        if (dayOfWeek > 7) {
          dayOfWeek = 1;
        } else if (dayOfWeek < 1) {
          dayOfWeek = 7;
        }
      }
      else if (editStep == 5) {
        isSummerTime = !isSummerTime;
      }
    } else if (currentPage == "towerTime") {
      if (editStep == 1) {
        towerHour = towerHour + change;
        if (towerHour > 23) {
          towerHour = 0;
        } else if (towerHour < 0) {
          towerHour = 23;
        }
      } else if (editStep == 2) {
        towerMinute = towerMinute + change;
        if (towerMinute > 59) {
          towerMinute = 0;
        } else if (towerMinute < 0) {
          towerMinute = 59;
        }
      }
    } else if (currentPage == "delay") {
      relayDelay = relayDelay + (change*10);
      if (delay > 4999) {
        relayDelay = 0;
      } else if (relayDelay < 0) {
        relayDelay = 4999;
      }
    }
  } else {
    changeScreen(change);
  }
  updateDisplay();
  showBacklight();
}

void changeScreen(int change) {
  currentPageIndex = currentPageIndex + change;
  if (currentPageIndex < 0) {
    currentPageIndex = NUM_OF_PAGES - 1;
  } else if (currentPageIndex >= NUM_OF_PAGES) {
    currentPageIndex = 0;
  }
}

void encoderPressed() {
  showBacklight();
  String currentPage = screenPages[currentPageIndex];
  if (editMode) {
    editStep++;
    if (currentPage == "sysTime") {
      if (editStep == 3) {
        currentPageIndex = currentPageIndex+1;
        editStep = 1; // moving to the date update
      }
    } else if (currentPage == "sysDate") {
      if (editStep == 6) {
        currentPageIndex = currentPageIndex+1;
        editStep = 1; // moving to the tower time setup
      }
    } else if (currentPage == "towerTime") {
      if (editStep == 3) {
        currentPageIndex = currentPageIndex+1;
        editStep = 1; // moving to the delay setup
      }
    } else if (currentPage == "delay") {
      myRTC.setHour(hour);
      myRTC.setMinute(minute);
      myRTC.setSecond(0);
      myRTC.setYear(year);
      myRTC.setMonth(month);
      myRTC.setDate(day);
      myRTC.setDoW(dayOfWeek);
      exitEditMode();
    }
  } else {
    if (currentPage == "sysTime" || currentPage == "sysDate" || currentPage == "towerTime" || currentPage == "delay") {
      editMode = true;
      currentPageIndex = 0; // go to the first screen to update all parameters
      editStep = 1;
    }
  }
  updateDisplay();
}

void exitEditMode() {
  editMode = false;
  editStep = 1;
}

// update the LCD screen with all the data
void updateDisplay() {
  lcd.clear();
  lcd.setCursor(0, 0);
  String currentPage = screenPages[currentPageIndex];
  if (editMode == true) {
    if (currentPage == "sysTime") {
      if (editStep == 1) {
        lcd.print("Set System Hour");
        lcd.setCursor(0, 1);
        lcd.print(hour);
      } else if (editStep == 2) {
        lcd.print("Set System Min.");
        lcd.setCursor(0, 1);
        lcd.print(minute);
      }
    } else if (currentPage == "sysDate") {
      if (editStep == 1) {
        lcd.print("Set System Day");
        lcd.setCursor(0, 1);
        lcd.print(day);
      } else if (editStep == 2) {
        lcd.print("Set System Month");
        lcd.setCursor(0, 1);
        lcd.print(month);
      } else if (editStep == 3) {
        lcd.print("Set System Year");
        lcd.setCursor(0, 1);
        lcd.print(year);
      } else if (editStep == 4) {
        lcd.print("Set Day Of Week");
        lcd.setCursor(0, 1);
        lcd.print(getWeekDayName());
      } else if (editStep == 5) {
        lcd.print("Is Summer Time");
        lcd.setCursor(0, 1);
        lcd.print(isSummerTime?"Yes":"No");
      }
    } else if (currentPage == "towerTime") {
      if (editStep == 1) {
        lcd.print("Set Tower Hour");
        lcd.setCursor(0, 1);
        lcd.print(towerHour);
      } else if (editStep == 2) {
        lcd.print("Set Tower Min.");
        lcd.setCursor(0, 1);
        lcd.print(towerMinute);
      }
    } else if (currentPage == "delay") {
      lcd.print("Set Relay delay");
      lcd.setCursor(0, 1);
      lcd.print(relayDelay);
    }
  } else {
    if (currentPage == "sysTime") {
      lcd.print("System Time");
      lcd.setCursor(0, 1);
      lcd.print(getFormatedTime(hour, minute, second)+" "+(isSummerTime?"Summer":"Winter"));
    } else if (currentPage == "sysDate") {
      lcd.print("System Date");
      lcd.setCursor(0, 1);
      lcd.print(getFormatedDate(year, month, day)+" "+getWeekDayName());
    } else if (currentPage == "towerTime") {
      lcd.print("Tower time");
      lcd.setCursor(0, 1);
      lcd.print(getFormatedShortTime(towerHour, towerMinute));
    } else if (currentPage == "temp") {
      lcd.print("Temperature");
      lcd.setCursor(0, 1);
      lcd.print(lastTemp);
    } else if (currentPage == "minTemp") {
      lcd.print("Min. T. " + String(minTemp));
      lcd.setCursor(0, 1);
      lcd.print(minTempDate);
    } else if (currentPage == "maxTemp") {
      lcd.print("Max. T. " + String(maxTemp));
      lcd.setCursor(0, 1);
      lcd.print(maxTempDate);
    } else if (currentPage == "delay") {
      lcd.print("Relay Delay");
      lcd.setCursor(0, 1);
      lcd.print(relayDelay);
    } else if (currentPage == "power") {
      lcd.print("Power Supply");
      lcd.setCursor(0, 1);
      lcd.print(isGridPowerOn() ? "Ok" : "Fail");
    }
  }
}

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

// add leading zero if reguired and semicolon
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

// get formated input time with hours minutes and seconds
String getFormatedTime(int hour, int minute, int second) {
  return printDigits(hour, false) + printDigits(minute, true) + printDigits(second, true);
}

// get formated input date with year, month, day
String getFormatedDate(int year, int month, int day) {
  return String(day) + "." + String(month) + ".20" + String(year);
}

// display only hours and minutes
String getFormatedShortTime(int hour, int minute) {
  return printDigits(hour, false) + printDigits(minute, true);
}

void checkTemperature() {
  lastTemp = myRTC.getTemperature();
  if (lastTemp > maxTemp) {
    maxTemp = lastTemp;
    maxTempDate = getFormatedDate(year, month, day) + " " + getFormatedShortTime(hour, minute);
  }
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

int getTowerMinutes() {
  int currentHour = towerHour % 12;  // if hour is in 24h format is the same for tower clock to use 12 hour format
  int dayMinutes = currentHour * 60 + towerMinute;
  if (dayMinutes==719){
    dayMinutes = -1;
  }
  return dayMinutes;
}
