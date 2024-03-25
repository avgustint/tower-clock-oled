
#include <Bounce2.h>

#define CLK_PIN 2           // rotary encoder clock pin
#define SW_PIN 3            // rotary encoder switch pin
#define DT_PIN 4            // rotary endoder data pin
#define RELAY_PIN 5         // trigger relay pin
#define POWER_CHECK_PIN 6   // checking there is power supply

// system time data
int year = 24;
int month = 1;
int day = 1;
int hour = 0;
int minute = 0;
int second = 0;

// tower time
int towerHour = 0;
int towerMinute = 0;

int lastTemp = 0;
int minTemp = 999;
int maxTemp = -999;
String minTempDate = "";
String maxTempDate = "";

Bounce debouncer = Bounce(); // Create a Bounce object
const int NUM_OF_PAGES = 8;
String screenPages[NUM_OF_PAGES] = ["sysTime", "sysDate", "towerTime", "temp", "min-temp", "max-temp", "delay", "power"];
int currentPageIndex = 0; 
bool editMode = false;
int editStep = 1;

int delay = 500;

// run the controller setup
void setup(){
  pinMode(CLK_PIN, INPUT);
  pinMode(DT_PIN, INPUT);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(POWER_CHECK_PIN, INPUT);
  debouncer.attach(SW_PIN); // Attach the button pin
  debouncer.interval(20);   // Set debounce interval (in milliseconds)
  attachInterrupt(digitalPinToInterrupt(CLK_PIN), encoderRotated, RISING);
  attachInterrupt(digitalPinToInterrupt(SW_PIN),  encoderPressed, RISING);
  Serial.begin(9600);
}

// run the controller loop
void loop(){
  debouncer.update(); // Update the debouncer
  // Read debounced button state
  if (debouncer.fell()) {
      Serial.println("Button pressed!");
  }
  if (editMode){
    updateDisplay();
  }
  else{
    updateTowerClock();
    delay(1000);
  }
}

void encoderRotated(){
  int change = 0;
  int dtState = digitalRead(DT_PIN);
  if (dtState == HIGH) {
      change = 1; //rotated clockwise
  } else {
      change = -1; // rotated anticlockwise
  }
  if (editMode){
    String currentPage = screenPages[currentPageIndex];
    if (currentPage=="sysTime"){
      if (editStep==1){
        hour=hour+change;
        if (hour>23){
          hour = 0;
        }
        else if (hour<0){
          hour = 23;
        }
      }
      else if (editStep==2){
        minute=minute+change;
        if (minute>59){
          minute = 0;
        }
        else if (minute<0){
          minute = 59;
        }
      }
    }
    else if (currentPage=="sysDate"){
      if (editStep==1){
        day=day+change;
        if (day>31){
          day = 0;
        }
        else if (day<1){
          day = 31;
        }
      }
      else if (editStep==2){
        month=month+change;
        if (month>12){
          month = 1;
        }
        else if (month<1){
          month = 12;
        }
      }
      else if (editStep==3){
        year=year+change;
        if (year>99){
          year = 0;
        }
        else if (year<0){
          year = 99;
        }
      }
    }
    else if (currentPage=="towerTime"){
      if (editStep==1){
        towerHour=towerHour+change;
        if (towerHour>11){
          towerHour = 0;
        }
        else if (towerHour<0){
          towerHour = 11;
        }
      }
      else if (editStep==2){
        towerMinute=towerMinute+change;
        if (towerMinute>59){
          towerMinute = 0;
        }
        else if (towerMinute<0){
          towerMinute = 59;
        }
      }
    }
    else if (currentPage=="delay"){
      delay=delay+change;
      if (delay>4999){
        delay = 0;
      }
      else if (delay<0){
        delay = 4999;
      }
    }
  }
  else{
    changeScreen(change);
  }
}

void changeScreen(int change){
  currentPageIndex = currentPageIndex + change;
  if (currentPageIndex<0){
    currentPageIndex = NUM_OF_PAGES - 1;
  }
  else if (currentPageIndex>=NUM_OF_PAGES){
    currentPageIndex = 0;
  }
}

void encoderPressed(){
  debouncer.update(); // Update the debouncer on interrupt
  Serial.println("Encoder pressed ... ");
  if (editMode){
    editStep++;
    String currentPage = screenPages[currentPageIndex];
    if (currentPage=="sysTime"){
      if (editStep == 3){
        rtc.setTime();
        exitEditMode();
      }
    }
    else if (currentPage=="sysDate"){
      if (editStep == 3){
        rtc.setDate();
        exitEditMode();
      }
    }
    else if (currentPage=="towerTime"){
      if (editStep == 3){
        exitEditMode();
      }
    }
    else if (currentPage=="delay"){
      exitEditMode();
    }
  }
  else {
    editMode = true;
    editStep = 1;
  }
}

void exitEditMode(){
  editMode=false;
  editStep = 1;
  updateDisplay();
}

void updateTowerClock(){
  checkTemperature();
  int currentDayMinutes = getSystemMinutes();   // using system time - get number of minutes in day for 12h format
  int towerDayMinutes = getTowerMinutes();      // using tower time - get number of minutes in day  of tower time for 12h format
  // calculate minutes diff between controller time and tower time
  int minutesDiff = currentDayMinutes - towerDayMinutes;
  // minutesDiff > 0 curent time is ahead of tower time - try to catch up, but only if current time is less then 6 hours ahead, otherwise wait
  int sixHoursDiff = 6 * 60;
  if (minutesDiff > 0 && minutesDiff < sixHoursDiff)
  {
    // need to turn the clock
    if (turnTheClock()){
      //
    }
  }
  updateDisplay();
}

void checkTemperature(){
  lastTemp = rtc.getTemperature();
  if (lastTemp>maxTemp){
    maxTemp = lastTemp;
    maxTempDate = getFormatedDate(year, month, day)+" "+getFormatedShortTime(hour, minute);
  }
  if (lastTemp<minTemp){
    minTemp = lastTemp;
    minTempDate = getFormatedDate(year, month, day)+" "+getFormatedShortTime(hour, minute);
  }
}

// try to turn the clock on tower
bool turnTheClock(){
  // check we have power supply and not running on battery - when there is no power we can not run the motor
  if (isGridPowerOn()){
    // open relay
    digitalWrite(RELAY_PIN, HIGH);
    delay(delay);
    // close relay
    digitalWrite(RELAY_PIN, LOW);
    // increment the tower time
    incrementTowerClock();
    return true;
  }
  return false;
}

// increment one minute of tower clock
incrementTowerClock(){
  // increment for 1 minute
  towerMinute = towerMinute + 1;
  // check there is no overflow
  if (towerMinute == 60)
  {
    towerHour = towerHour + 1;
    towerMinute = 0;
    if (towerHour == 12)
    {
      towerHour = 0
    }
  }
}

// function to check if we have power supply - reading pin value used to detect power supply
bool isGridPowerOn(){
  int powerOn = digitalRead(POWER_CHECK_PIN);
  return powerOn == HIGH ? true : false;
}

// get number of minutes in a day by using only 12h format - because on tower we have only 12 hours
int getSystemMinutes(){
  int currentHour = hour % 12; // if hour is in 24h format is the same for tower clock to use 12 hour format
  return currentDayMinutes = currentHour * 60 + minute;
}

int getTowerMinutes(){
  int currentHour = towerHour % 12; // if hour is in 24h format is the same for tower clock to use 12 hour format
  return currentDayMinutes = currentHour * 60 + towerMinute;
}

// update the LCD screen with all the data
void updateDisplay(){
  lcd.setCursor(0,0);
  String currentPage = screenPages[currentPageIndex];
  if (editMode==false){
    if (currentPage=="sysTime"){
      if (editStep==1){
        lcd.println("Set System Hour");
        lcd.setCursor(0,1);
        lcd.println(hour);
      }
      else if (editStep==2){
        lcd.println("Set System Min.");
        lcd.setCursor(0,1);
        lcd.println(minute);
      }
    }
    else if (currentPage=="sysDate"){
      if (editStep==1){
        lcd.println("Set System Day");
        lcd.setCursor(0,1);
        lcd.println(day);
      }
      else if (editStep==2){
        lcd.println("Set System Month");
        lcd.setCursor(0,1);
        lcd.println(month);
      }
      else if (editStep==3){
        lcd.println("Set System Year");
        lcd.setCursor(0,1);
        lcd.println(year);
      }
    }
    else if (currentPage=="towerTime"){
      if (editStep==1){
        lcd.println("Set Tower Hour");
        lcd.setCursor(0,1);
        lcd.println(towerHour);
      }
      else if (editStep==2){
        lcd.println("Set Tower Min.");
        lcd.setCursor(0,1);
        lcd.println(towerMinute);
      }
    }
    else if (currentPage=="delay"){
      lcd.println("Set Relay delay");
      lcd.setCursor(0,1);
      lcd.println(delay);
    }
  }
  else{
    if (currentPage=="sysTime"){
      lcd.println("System Time");
      lcd.setCursor(0,1);
      lcd.println(getFormatedTime(hour, minute, second));
    }
    else if (currentPage=="sysDate"){
      lcd.println("System Date");
      lcd.setCursor(0,1);
      lcd.println(getFormatedDate(year, month, day));
    }
    else if (currentPage=="towerTime"){
      lcd.println("Tower time");
      lcd.setCursor(0,1);
      lcd.println(getFormatedShortTime(towerHour, towerMinute));
    }
    else if (currentPage=="temp"){
      lcd.println("Temperature");
      lcd.setCursor(0,1);
      lcd.println(lastTemp);
    }
    else if (currentPage=="minTemp"){
      lcd.println("Min. T. "+minTemp);
      lcd.setCursor(0,1);
      lcd.println(minTempDate);
    }
    else if (currentPage=="maxTemp"){
      lcd.println("Max. T. "+maxTemp);
      lcd.setCursor(0,1);
      lcd.println(maxTempDate);
    }
    else if (currentPage=="delay"){
      lcd.println("Relay Delay");
      lcd.setCursor(0,1);
      lcd.println(delay);
    }
    else if (currentPage=="power"){
      lcd.println("Power Supply");
      lcd.setCursor(0,1);
      lcd.println(isGridPowerOn()?"Ok":"Fail");
    }
  }

}

// add leading zero if reguired and semicolon
String printDigits(int digits, bool addSemicolumn){
  // utility function for digital clock display: prints preceding colon and leading 0
  String output = "";
  if (addSemicolumn)
    output = ":";
  if (digits < 10)
    output = output + "0";
  output = output + String(digits);
  return output
}

// get formated input time with hours minutes and seconds
String getFormatedTime(int hour, int minute, int second){
  return printDigits(hour, false) + printDigits(minute, true) + printDigits(second, true);
}

// get formated input date with year, month, day
String getFormatedTime(int year, int month, int day){
  return day+"."+month+".20"+ year;
}

// display only hours and minutes
String getFormatedShortTime(int hour, int minute){
  return printDigits(hour, false) + printDigits(minute, true);
}