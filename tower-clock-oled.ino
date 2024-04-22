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
// #include <Bounce2.h>

// rotary encoder pins
#define CLK_PIN 2 // rotary encoder clock pin
#define SW_PIN 3  // rotary encoder switch pin
#define DT_PIN 8  // rotary encoder data pin

// OLED display options
#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // I2C device address

// other pins
#define POWER_CHECK_PIN 10 // pin for checking power supply
#define RELAY_1_PIN 14     // define pin to trigger the relay 1
#define RELAY_2_PIN 15     // define pin to trigger the relay 2

#define MOTOR_DELAY 5000           // motor delay between next time changing state
#define NO_ACTIVITY_DURATION 60000 // the duration of waiting time in edit mode after which we auto close edit mode without changes or turning display into sleep

// system time data
uint16_t year;
uint8_t month;
uint8_t day;
uint8_t hour;
uint8_t minute;
uint8_t second;
uint8_t dayOfWeek;
bool isSummerTime = false; // are we using summer or winter time depending on DST rules

// tower time - the current position of tower clock indicators
uint8_t towerHour = 0;
uint8_t towerMinute = 0;

bool channel1Open = false;                  // next state of relay - toggle between 2 solid state relay
uint32_t currentSeconds;                    // 32-bit times as seconds since 2000-01-01.
uint32_t lastUpdateSeconds;                 // 32-bit times as seconds since 2000-01-01.
unsigned long lastUpdateEditModeScreen = 0; // when we last updated screen in edit mode for blinking
DateTime currentTime;                       // list read time from RTC
// char lastTimeStartup[24];
bool editMode = false;                  // are we in edit mode
bool editModeBlinkDark = false;         // toggle blinking in edit mode when value selected
unsigned long lastEditModeChange = 0;   // stored time when user done some interaction in edit mode - auto cancel edit mode after timeout
uint8_t mainScreenIndex = 1;            // index of displayed screen in normal operation - switching between different screens
unsigned long last_clock_interrupt = 0; // variable used for detecting rotary encoder rotation

// motor
bool motorRotating = false;       // current state of the motor - true: rotating, false: motor not rotating any more
unsigned long motorOpenStart = 0; // when motor started to rotate

// temperature - record current, min and max temperature and time when recorded
int8_t lastTemp;
int8_t minTemp = 120;
int8_t maxTemp = -120;
char minTempDate[24];
char maxTempDate[24];

// RTC module self compensation
uint16_t compensateAfterDays = 0;
int8_t compensateSeconds = 0; // possible values -1s, 0 or +1s
unsigned long secondsElapsedSinceLastCompensation = 0;
unsigned long secondsToCompensate = 0;
// char lastTimeCompensated[24];
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
// char lastTimeSetup[24]

// initialize objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RTC_DS3231 myRTC;
// Bounce debouncerSwitch = Bounce(); // Create a Bounce object to prevent any glitches from rotary encoder switch
// Bounce debouncerClk = Bounce();    // Create a Bounce object to prevent any glitches from rotary encoder clock signal

// initialize the controller
void setup()
{
    // Serial.begin(9600);
    // delay(1000); // wait for console opening
    if (!myRTC.begin())
    {
        // Serial.println(F("Couldn't find RTC"));
        while (1)
            ;
    }

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        // Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ; // Don't proceed, loop forever
    }
    // setup controller pins
    pinMode(CLK_PIN, INPUT_PULLUP);
    pinMode(DT_PIN, INPUT_PULLUP);
    pinMode(SW_PIN, INPUT_PULLUP);
    pinMode(POWER_CHECK_PIN, INPUT);
    pinMode(RELAY_1_PIN, OUTPUT);
    pinMode(RELAY_2_PIN, OUTPUT);
    digitalWrite(RELAY_1_PIN, HIGH); // turn on the relay 1
    digitalWrite(RELAY_2_PIN, LOW);  // turn off the relay 2
    // debouncerSwitch.attach(SW_PIN);  // Attach to the rotary encoder press pin
    // debouncerSwitch.interval(2);     // Set debounce interval (in milliseconds)
    // debouncerClk.attach(CLK_PIN);    // Attach to the rotary encoder clock pin
    // debouncerClk.interval(2);        // Set debounce interval (in milliseconds)

    getCurrentTime();
    // sprintf(lastTimeStartup, "%02d.%02d.%4d %02d:%02d:%02d", currentTime.day(), currentTime.month(), currentTime.year(), currentTime.hour(), currentTime.minute(), currentTime.second());
    checkTemperature();
    isSummerTime = checkIsSummerTime(); // check if currently is Summer or Winter time depending to DST rules
    // initialize tower clock on same as current time  - will be set correctly in edit mode - here just to prevent rotating the tower clock
    towerHour = hour;
    towerMinute = minute;
    display.clearDisplay();
    display.display(); // this command will display all the data which is in buffer
    display.setTextWrap(false);

    // Serial.println(F("Initialization end"));
    // Attach interrupts
    attachInterrupt(digitalPinToInterrupt(CLK_PIN), encoderRotated, FALLING);
    attachInterrupt(digitalPinToInterrupt(SW_PIN), encoderPressed, FALLING);
}

// main microcontroller loop
void loop()
{
    // Update the debouncer
    // debouncerSwitch.update();
    // debouncerClk.update();

    // Read debounced rotary encoder press button state
    // if (debouncerSwitch.fell())
    //{
    //    encoderPressed();
    //}

    // Read debounced rotary encoder rotating clock event
    // if (debouncerClk.fell())
    //{
    //    encoderRotated();
    //}

    getCurrentTime(); // read current time from RTC module
    if (editMode)
    {
        if (checkTimeoutExpired(lastUpdateEditModeScreen, 300))
        {
            editModeBlinkDark = !editModeBlinkDark;
            lastUpdateEditModeScreen = millis();
            updateEditModeDisplay();
        }
        // check auto exit edit mode timeout already passed
        if (checkTimeoutExpired(lastEditModeChange, NO_ACTIVITY_DURATION))
        {
            exitEditMode(); // auto exit edit mode after no interaction timeout
        }
    }
    else
    {
        if (lastUpdateSeconds != currentSeconds)
        {
            lastUpdateSeconds = currentSeconds;
            updateTowerClock();
            updateMainScreen();
            checkNeedToCompensateTime();
            if (checkTimeoutExpired(lastEditModeChange, NO_ACTIVITY_DURATION))
            {
                mainScreenIndex = 0; // close display
            }
        }
    }
    checkCloseMotorRelay(); // check rotating delay expired
}

// reset function to have ability to reset Arduino programmatically
void (*resetFunc)(void) = 0;

// read the current time from the RTC module
void getCurrentTime()
{
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
void updateTowerClock()
{
    uint16_t currentDayMinutes = getSystemMinutes();            // using system time - get number of minutes in day for 12h format
    uint16_t towerDayMinutes = getTowerMinutes();               // using tower time - get number of minutes in day  of tower time for 12h format
    uint16_t minutesDiff = currentDayMinutes - towerDayMinutes; // calculate minutes diff between controller time and tower time
    // minutesDiff > 0 curent time is ahead of tower time - try to catch up, but only if current time is less then 10 hours ahead, otherwise wait
    uint16_t sixHoursDiff = 10 * 60;
    if (minutesDiff > 0 && minutesDiff < sixHoursDiff)
    {
        turnTheClock();     // need to turn the tower clock
        checkTemperature(); // every minute read also RTC module temperature
    }
}

// Turn the tower clock if there is power supply from grid, because otherwise motor will not rotate when relay is triggered
void turnTheClock()
{
    // check we have power supply and not running on battery - when there is no power we can not run the motor
    // check also not already in motor rotating state
    if (motorRotating == false && isGridPowerOn())
    {
        incrementTowerClock();                   // increment the tower time fist to display correct new time on lcd screen
        motorRotating = true;                    // set variable that relay is open and motor is in rotating state
        motorOpenStart = millis();               // store time when we start rotating the motor to stop after timeout
        digitalWrite(RELAY_1_PIN, channel1Open); // internate both relay between  different states
        channel1Open = !channel1Open;            // toggle the state
        digitalWrite(RELAY_2_PIN, channel1Open); // internate both relay between different states
    }
}

// increment one minute of tower clock
void incrementTowerClock()
{
    // increment for 1 minute
    towerMinute = towerMinute + 1;
    // check there is no overflow
    if (towerMinute == 60)
    {
        towerHour = towerHour + 1;
        towerMinute = 0;
        if (towerHour >= 24)
        {
            towerHour = 0;
        }
    }
}

// check if we need to close the relay or motor stopped rotating
// calculate time difference instead of using the delays
// do not trigger the relay for next 5 seconds - to not rotate too quickly when hour change on DST also motor is rotating longer then relay is open - motor is having own relay to turn off when rotated enough
void checkCloseMotorRelay()
{
    if (motorRotating && checkTimeoutExpired(motorOpenStart, MOTOR_DELAY))
    {
        motorRotating = false;
    }
}

// read the current temperature from the RTC module - the precision of the temperature sensor is ± 3°C
void checkTemperature()
{
    // read current temperature
    lastTemp = myRTC.getTemperature();
    // check current temperature exceeds max recorded temp
    if (lastTemp > maxTemp)
    {
        maxTemp = lastTemp;
        sprintf(maxTempDate, "%02d.%02d.%4d %02d:%02d", currentTime.day(), currentTime.month(), currentTime.year(), currentTime.hour(), currentTime.minute());
    }
    // check current temp is lower them min recorded temp
    if (lastTemp < minTemp)
    {
        minTemp = lastTemp;
        sprintf(minTempDate, "%02d.%02d.%4d %02d:%02d", currentTime.day(), currentTime.month(), currentTime.year(), currentTime.hour(), currentTime.minute());
    }
}

// common function to check if different timeouts already expired from last change
bool checkTimeoutExpired(unsigned long lastChange, unsigned int timeoutDuration)
{
    unsigned long currentMillis = millis();
    return ((currentMillis - lastChange) >= timeoutDuration);
}

// get number of minutes in a day by using only 12h format - because on tower we have only 12 hours
uint16_t getSystemMinutes()
{
    uint16_t currentHour = hour % 12; // if hour is in 24h format is the same for tower clock to use 12 hour format
    return currentHour * 60 + minute;
}

// get number of minutes of tower clock in a day by using only 12h format
uint16_t getTowerMinutes()
{
    uint16_t currentHour = towerHour % 12; // if hour is in 24h format is the same for tower clock to use 12 hour format
    uint16_t dayMinutes = currentHour * 60 + towerMinute;
    if (dayMinutes == 719)
    { // if last minute in a day then return -1, because first minute in a day using system time will be zero, otherwise tower clock will not be incremented
        dayMinutes = -1;
    }
    return dayMinutes;
}

// display main screen info in normal operation - switch between different screens
void updateMainScreen()
{
    display.fillRect(0, 0, 128, 64, SSD1306_BLACK);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    if (mainScreenIndex == 1)
    {
        addStatusInfo();
        display.setTextSize(1);
        display.setCursor(0, 22);
        display.setTextColor(WHITE);
        char currentDate[16];
        sprintf(currentDate, "%02d.%02d.%4d", day, month, year); // add leading zeros to the day and month
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
        if (motorRotating)
        {
            char rotatingText[10] = "Rotating";
            display.setCursor(50, 56);
            display.print(rotatingText);
        }
    }
    else if (mainScreenIndex == 2)
    {
        // temperature details
        display.setCursor(0, 0);
        char temperatureText[12] = "Temperature";
        display.print(temperatureText);
        display.setTextSize(2);
        display.setCursor(0, 10);
        display.print(getTemp(lastTemp));
        display.setTextSize(1);
        display.setCursor(0, 27);
        char minText[6] = "Min:";
        display.print(minText);
        display.setCursor(66, 27);
        display.print(getTemp(minTemp));
        display.setCursor(0, 37);
        display.print(minTempDate);
        display.setCursor(0, 47);
        char maxText[6] = "Max:";
        display.print(maxText);
        display.setCursor(66, 47);
        display.print(getTemp(maxTemp));
        display.setCursor(0, 57);
        display.print(maxTempDate);
    }
    else if (mainScreenIndex == 3)
    {
        // // time compensation data
        // display.setCursor(0, 0);
        // char compensateAfterText[18] = "Compensate after";
        // display.print(compensateAfterText);
        // display.setCursor(0, 10);
        // display.print(compensateAfterDays);
        // display.setCursor(30, 10);
        // char daysText[6] = "days";
        // display.print(daysText);
        // display.setCursor(60, 10);
        // display.print(compensateSeconds);
        // display.setCursor(80, 10);
        // char secondsText[8] = "seconds";
        // display.print(secondsText);
        // display.setCursor(0, 20);
        // char lastCompensationText[20] = "Last compensation:";
        // display.print(lastCompensationText);
        // display.setCursor(0, 30);
        // // display last date time that clock has been self adjusted

        // if (totalSecondsCompensated == 0) {
        //   char neverText[6] = "Never";
        //   display.print(neverText);
        // } else {
        //   display.print(lastTimeCompensated);
        // }
        // display.setCursor(0, 40);
        // char totalCompensatedText[20] = "Total compensated:";
        // display.print(totalCompensatedText);
        // display.setCursor(0, 50);
        // display.print(totalSecondsCompensated);
        // display.setCursor(40, 50);
        // char secondsCompensatedText[8] = "seconds";
        // display.print(secondsCompensatedText);

        // char lastStartupText[14] = "Last Startup";
        // display.print(lastStartupText);
        // display.setCursor(0, 10);
        // display.print(lastTimeStartup);
        // display.setCursor(0, 20);
        // char lastSetupText[12] = "Last Setup";
        // display.print(lastSetupText);
        // display.setCursor(0, 30);
        // display.print(lastTimeSetup);
    }
    display.display();
}

// display status bar information
void addStatusInfo()
{
    display.setTextSize(1);
    // display power status
    display.setCursor(0, 0);
    char powerText[12];
    isGridPowerOn() ? powerText = "Power: OK" : powerText = "Power: Fail";
    display.print(powerText);
    // display temperature
    display.setCursor(80, 0);
    display.print(getTemp(lastTemp));
    // display Summer/Winter Time
    display.setCursor(0, 10);
    char seasonText[8];
    isSummerTime ? seasonText = "Summer" : seasonText = "Winter";
    display.print(seasonText);
    // display open SSR channel
    display.setCursor(80, 10);
    char channelText[6];
    channel1Open ? channelText = "CH 1" : channelText = "CH 2";
    display.print(channelText);
}

// show screen in edit mode
void updateEditModeDisplay()
{
    display.fillRect(0, 0, 128, 64, SSD1306_BLACK);
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    char systemText[8] = "System:";
    display.print(systemText);
    display.setCursor(68, 0);
    char editDate[16];
    sprintf(editDate, "%02d.%02d.%4d", edit_day, edit_month, edit_year); // add leading zeros to the day and month
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
    display.setCursor(0, 36);
    char compensateText[12] = "Compensate";
    display.print(compensateText);
    display.setCursor(68, 36);
    display.print(edit_compensateAfterDays);
    display.setCursor(90, 36);
    char daysText[2] = "d";
    display.print(daysText);
    display.setCursor(100, 36);
    display.print(edit_compensateSeconds);
    display.setCursor(112, 36);
    char secondsText[2] = "s";
    display.print(secondsText);
    display.setCursor(0, 54);
    char resetText[6] = "Reset";
    display.print(resetText);
    display.setCursor(40, 54);
    char confirmText[8] = "Confirm";
    display.print(confirmText);
    display.setCursor(90, 54);
    char cancelText[8] = "Confirm";
    display.print(cancelText);
    // no item selected to edit  - draw line under option
    if (currentPageIndex != selectedPageIndex)
    {
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
        switch (currentPageIndex)
        {
        case 1: // day
            display.drawFastHLine(68, 8, 12, WHITE);
            break;
        case 2: // month
            display.drawFastHLine(85, 8, 12, WHITE);
            break;
        case 3: // year
            display.drawFastHLine(104, 8, 24, WHITE);
            break;
        case 4: // hour
            display.drawFastHLine(68, 20, 12, WHITE);
            break;
        case 5: // minute
            display.drawFastHLine(85, 20, 12, WHITE);
            break;
        case 6: // tower hour
            display.drawFastHLine(68, 32, 12, WHITE);
            break;
        case 7: // tower minute
            display.drawFastHLine(85, 32, 12, WHITE);
            break;
        case 8: // compensate after days
            display.drawFastHLine(68, 44, 18, WHITE);
            break;
        case 9: // compensate seconds
            display.drawFastHLine(100, 44, 10, WHITE);
            break;
        case 10: // reset
            display.drawFastHLine(0, 63, 30, WHITE);
            break;
        case 11: // confirm
            display.drawFastHLine(40, 63, 41, WHITE);
            break;
        case 12: // cancel
            display.drawFastHLine(90, 63, 35, WHITE);
            break;
        }
    }
    else
    {
        // one of options selected to edit - blink that option
        if (editModeBlinkDark)
        {
            switch (currentPageIndex)
            {
            case 1: // day
                display.fillRect(68, 0, 12, 10, SSD1306_BLACK);
                break;
            case 2: // month
                display.fillRect(85, 0, 12, 10, SSD1306_BLACK);
                break;
            case 3: // year
                display.fillRect(104, 0, 24, 10, SSD1306_BLACK);
                break;
            case 4: // hour
                display.fillRect(68, 12, 12, 10, SSD1306_BLACK);
                break;
            case 5: // minute
                display.fillRect(85, 12, 12, 10, SSD1306_BLACK);
                break;
            case 6: // tower hour
                display.fillRect(68, 24, 12, 10, SSD1306_BLACK);
                break;
            case 7: // tower minute
                display.fillRect(85, 24, 12, 10, SSD1306_BLACK);
                break;
            case 8: // compensate after days
                display.fillRect(68, 36, 18, 10, SSD1306_BLACK);
                break;
            case 9: // compensate seconds
                display.fillRect(100, 36, 10, 10, SSD1306_BLACK);
                break;
            }
        }
    }
    display.display();
}

// format temperature with degrees symbol
String getTemp(int8_t temp)
{
    return String(temp) + " " + (char)247 + F("C");
}

// function to check if we have power supply - reading pin value used to detect power supply
bool isGridPowerOn()
{
    return digitalRead(POWER_CHECK_PIN);
}

// calculate current date is using Summer or Winter DST rules
bool checkIsSummerTime()
{
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
    uint8_t previousSunday = day - dayOfWeek % 7; // use different indexes the app 0 - Sunday, 1-Monday,..., 6-Saturday

    if (month == 3)
        return previousSunday >= 25;
    if (month == 10)
        return previousSunday < 25;

    return false; // this line never gonna happened
}

// user presses encoder button
void encoderPressed()
{
    lastEditModeChange = millis();
    if (editMode)
    {
        if (currentPageIndex == 10)
        {
            // reset
            resetFunc();
        }
        else if (currentPageIndex == 11)
        {
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
            isSummerTime = checkIsSummerTime(); //
            // sprintf(lastTimeSetup, "%02d.%02d.%4d %02d:%02d:%02d", currentTime.day(), currentTime.month(), currentTime.year(), currentTime.hour(), currentTime.minute(), currentTime.second());
            exitEditMode(); // move to normal operational mode
        }
        else if (currentPageIndex == 12)
        {
            // cancel
            exitEditMode();
        }
        else
        {
            if (currentPageIndex != selectedPageIndex)
            {
                // select currently underlined option
                selectedPageIndex = currentPageIndex;
            }
            else
            {
                // deselect currently selected option
                selectedPageIndex = 0;
            }
        }
    }
    else
    {
        // currently not in edit mode and button pressed
        enterEditMode();
        updateEditModeDisplay();
    }
}

// event when user is rotating the rotary encoder button
void encoderRotated()
{
    lastEditModeChange = millis();
    uint8_t change = 0;
    if (lastEditModeChange - last_clock_interrupt > 5)
    {
        if (digitalRead(DT_PIN) == 1)
        {
            change = 1; // rotated clockwise
        }
        if (digitalRead(DT_PIN) == 0)
        {
            change = -1; // rotated anticlockwise
        }
        last_clock_interrupt = lastEditModeChange();
    }

    // int dtState = digitalRead(DT_PIN);
    // if (dtState == HIGH)
    // {
    //     change = 1; // rotated clockwise
    // }
    // else
    // {
    //     change = -1; // rotated anticlockwise
    // }
    if (editMode)
    { // edit mode
        if (currentPageIndex != selectedPageIndex)
        {
            currentPageIndex = currentPageIndex + change;
            currentPageIndex = checkRange(currentPageIndex, 1, 12);
        }
        else
        {
            switch (currentPageIndex)
            {
            case 1: // day
                    // update the system date day
                edit_day = edit_day + change;
                edit_day = checkRange(edit_day, 1, 31);
                break;
            case 2: // month
                    // update the system time month
                edit_month = edit_month + change;
                edit_month = checkRange(edit_month, 1, 12);
                break;
            case 3: // year
                    // update the system time year - changing only last 2 digits
                edit_year = edit_year + change;
                edit_year = checkRange(edit_year, 2024, 2099); // minimum year can be current year 2024
                break;
            case 4: // hour
                    // update the system time hour
                edit_hour = edit_hour + change;
                edit_hour = checkRange(edit_hour, 0, 23);
                break;
            case 5: // minute
                    // update the system time minutes
                edit_minute = edit_minute + change;
                edit_minute = checkRange(edit_minute, 0, 59);
                break;
            case 6: // tower hour
                    // updating the tower clock hour
                edit_towerHour = edit_towerHour + change;
                edit_towerHour = checkRange(edit_towerHour, 0, 23);
                break;
            case 7: // tower minute
                    // updating the tower clock minutes
                edit_towerMinute = edit_towerMinute + change;
                edit_towerMinute = checkRange(edit_towerMinute, 0, 59);
                break;
            case 8: // compensate after days
                edit_compensateAfterDays = edit_compensateAfterDays + change;
                edit_compensateAfterDays = checkRange(edit_compensateAfterDays, 0, 365);
                break;
            case 9: // compensate seconds
                edit_compensateSeconds = edit_compensateSeconds + change;
                edit_compensateSeconds = checkRange(edit_compensateSeconds, -1, 1);
                break;
            }
        }
        updateEditModeDisplay();
    }
    else
    {
        // rotate between main screens
        mainScreenIndex = mainScreenIndex + change;
        mainScreenIndex = checkRange(mainScreenIndex, 1, 2);
    }
}

// check changed value is inside the range
int checkRange(int value, int minValue, int maxValue)
{
    if (value > maxValue)
    {
        value = minValue;
    }
    else if (value < minValue)
    {
        value = maxValue;
    }
    return value;
}

// start edit mode - initialize some variables
void enterEditMode()
{
    editMode = true;
    currentPageIndex = 1; // go to the first item to change
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
void exitEditMode()
{
    currentPageIndex = 1;
    selectedPageIndex = 0;
    editMode = false;
    mainScreenIndex = 1;
}

// get day of week names from index 1-Starting on Monday and 7 as Sunday
String getWeekDayName(uint8_t dayIndex)
{
    switch (dayIndex)
    {
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
void checkNeedToCompensateTime()
{
    if ((compensateAfterDays > 0) && (compensateSeconds != 0) && (second == 30))
    { // compensate only in the middle of minute to prevent overflow to next minute, hour, day, month or even year
        if (secondsElapsedSinceLastCompensation > secondsToCompensate)
        {
            secondsElapsedSinceLastCompensation = 0;
            if (compensateSeconds > 0)
            {
                second++;
                totalSecondsCompensated++;
            }
            else
            {
                second--;
                totalSecondsCompensated--;
            }
            myRTC.adjust(DateTime(year, month, day, hour, minute, second)); //
            // sprintf(lastTimeCompensated, "%02d.%02d.%4d %02d:%02d", currentTime.day(), currentTime.month(), currentTime.year(), currentTime.hour(), currentTime.minute());
        }
    }
}

// check if we need to update the time because of DST rules in Europe
// changing the time on last Sunday in March and October
void checkDaylightSavingChanges()
{
    // check last Sunday in March or October and update the time accordingly
    // earliest possible date in 25. March or October and latest 31. March or October
    if (dayOfWeek == 7 && day >= 25)
    {
        if (month == 3 && hour == 2 && minute == 0)
        {
            // moving hour forward on 02:00 to 03:00
            hour = 3;
            myRTC.adjust(DateTime(year, month, day, hour, minute, second));
            isSummerTime = true;
        }
        // moving hour backwards from 03:00 to 2:00 - do this only once otherwise will be in the loop
        else if (month == 10 && hour == 3 && minute == 0 && isSummerTime)
        {
            hour = 2;
            myRTC.adjust(DateTime(year, month, day, hour, minute, second));
            isSummerTime = false;
        }
    }
}
