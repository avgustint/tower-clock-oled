# **Tower Clock Arduino Controller**

## **Introduction**

Controlling the clock in a church tower with an arduino micro controller using a real time clock (RTC) module that stores and returns the correct time. The controller attempts to synchronize the time of the clock in the tower with the system time of the controller. Every full minute, the tower clock must be moved forward so that the AC motor is running. The motor is triggered by the opening of a solid state relay. The tower clock shall be stopped, if there is no power in the mains to turn the motor. The clock in the tower will synchronize when we have power from the grid again.

Program is taking into consideration also the rules for Daylight Saving Time (DST) in Europe. On the last Sunday in March, time moves forward one hour from 2:00 to 3:00 and on the last Sunday in October, time moves back from 3:00 to 2:00. When the time moves forward, the tower clock will catch the correct time. When the time moves back, the tower clock will wait one hour.

The controller uses a rotary encoder with a push-button to set all the necessary variables (system time, current time of the tower clock, etc.). All settings and configurations are displayed on an OLED screen.

Use of lithium battery as UPS controller. The controller is powered by a 5 V DC supply connected to the mains. In case of mains failure, the lithium battery is switched over to the lithium battery with a lithium charging module, which also takes care of charging the battery.

The RTC module is temperature compensated. The RTC module reads and records the minimum/maximum temperature and displays it on the screen.

## **Wiring**

Wiring diagram how to connect all required components.

![image](https://github.com/avgustint/tower-clock-oled/blob/main/images/other/clock-oled-schematic.png)

## **Construction**

Connect all required components with Arduino Nano micro controller and enclose in appropriate housing.

TO-DO Add images

## **Setup**

Instruction how to setup the controller with the correct time and current tower time.

Press the rotary encoder button to enter setup mode. When in entering setup mode wizard starts to enter all required values step by step. Each click navigate wizard to the next configuration step. All inputted values are stored when confirming last step (relay delay). Confirm last step on full minute time to have exact time on seconds level. Rotate the rotary encoder button to change the values.

Step 1: Enter correct controller system hour.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9997.jpeg)

Step 2: Enter correct controller system minutes. On confirm step 0 seconds will be set.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9999.jpeg)

Step 3: Enter current day of month.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0001.jpeg)

Step 4: Enter current month.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0002.jpeg)

Step 5: Enter current year. Possible to insert in range from 2024 and 2099.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0026.jpeg)

Step 6: Enter day of the week (Monday, Tuesday, Wednesday,...)
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0004.jpeg)

Step 7: Enter tower clock current hour
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0006.jpeg)

Step 8: Enter tower clock current minute
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0007.jpeg)

Step 9: Enter relay opening state delay in milliseconds in step of 10 milliseconds.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0008.jpeg)

Step 10: Enter motor rotating wait time after triggering the relay in milliseconds in step of 10 milliseconds.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0021.jpeg)

Step 11: Confirm or Cancel inserted values to take affect.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0022.jpeg)

On confirming last step, values are stored and set. New time is now used and controller returns to normal operational mode. If user does not finish the setup process, then controller returns to normal operation after timeout (60 seconds) since last user interaction. This prevents staying in edit mode when user just accidentally clicks on encoder button. In edit mode relay trigger does not operate and tower clock would be stopped.

## **Usage**

When controller operates in normal mode, you can use rotary encoder button to display different information on OLED screen. When user interact with button, LCD backlight is switched on and remains on for 30 seconds from the last interaction. Following screens are available:

Showing current controller time and day of week.


## **Components**

### Controller Arduino Nano
![image](https://github.com/avgustint/tower-clock/assets/9412797/8fbae44a-66fd-4745-98ee-94f49e7eb06a)

### OLED display 2.42 Inch 128x64 SSD1309 I2C
TO-DO add image

### Rechargeable Lithium Battery LP103395 3.7V 3700mAh 13.7Wh
![image](https://github.com/avgustint/tower-clock/assets/9412797/0f3e59af-5f53-4cb3-9975-40ea88144ab4)

### Power Supply and Charging Controller
![image](https://github.com/avgustint/tower-clock/assets/9412797/9fa9ace6-a631-413e-a117-079d3b30c8a3)

### Real Time Clock Module DS3231
![image](https://github.com/avgustint/tower-clock/assets/9412797/990adb97-49cd-43ab-b6b3-29cf0235794e)

### Rotary Encoder with Press Button
![image](https://github.com/avgustint/tower-clock/assets/9412797/0cd3fe18-62a0-431a-86cb-1cd6314a8132)

### Solid State Relay
To-Do Add image

## **Tower Clock Motor** 

Few images from tower clock motor that rotates the clock indicators. Motor has also own encoder switch that toggles between open and closed state when rotated for one minute. 

Front view
![image](https://github.com/avgustint/tower-clock/blob/main/images/motor/IMG_9879.jpeg)

Side view
![image](https://github.com/avgustint/tower-clock/blob/main/images/motor/IMG_9880.jpeg)

Side view
![image](https://github.com/avgustint/tower-clock/blob/main/images/motor/IMG_9881.jpeg)

Top view
![image](https://github.com/avgustint/tower-clock/blob/main/images/motor/IMG_9882.jpeg)

Switch closeup
![image](https://github.com/avgustint/tower-clock/blob/main/images/motor/IMG_9883.jpeg)


## **Future improvements**

* Add Lora remote diagnostic and configuration options. Option to remote setup exact time from NTP server.


March 2024 
