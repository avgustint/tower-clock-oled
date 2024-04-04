# **Tower Clock Arduino Controller**

## **Introduction**

Controlling the clock in a church tower with an arduino micro controller using a real time clock (RTC) module that stores and returns the correct time. The controller attempts to synchronize the time of the clock in the tower with the system time of the controller. Every full minute, the tower clock must be moved forward so that the AC motor is running. The motor is triggered by the opening of a relay. The tower clock shall be stopped, if there is no power in the mains to turn the motor. The clock in the tower will synchronize when we have power from the grid again.

Program is taking into consideration also the rules for Daylight Saving Time (DST) in Europe. On the last Sunday in March, time moves forward one hour from 2:00 to 3:00 and on the last Sunday in October, time moves back from 3:00 to 2:00. When the time moves forward, the tower clock will catch the correct time. When the time moves back, the tower clock will wait one hour.

The controller uses a rotary encoder with a push-button to set all the necessary variables (system time, current time of the tower clock, relay delay, etc.). All settings and configurations are displayed on an LCD screen with automatic on/off illumination.

Use of lithium battery as UPS controller. The controller is powered by a 5 V DC supply connected to the mains. In case of mains failure, the lithium battery is switched over to the lithium battery with a lithium charging module, which also takes care of charging the battery.

The RTC module is temperature compensated. The RTC module reads and records the minimum/maximum temperature and displays it on LCD screen.

## **Wiring**

Wiring diagram how to connect all required components.

![image](https://github.com/avgustint/tower-clock/blob/main/images/other/clock-wiring.png)

## **Construction**

Connect all required components with Arduino Nano micro controller and enclose in appropriate housing.

![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0013.jpeg)

![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0014.jpeg)

![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0015.jpeg)

![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9985.jpeg)

## **Setup**

Instruction how to setup the controller with the correct time and current tower time.

Press the rotary encoder button to enter setup mode when on date or time screen. When entering setup mode wizard starts to enter all required values step by step. Each click navigate wizard to the next configuration step. All inputted values are stored when confirming last step (relay delay). Confirm last step on full minute time to have exact time on seconds level. Rotate the rotary encoder button to change the values.

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

When controller operates in normal mode, you can use rotary encoder button to display different information on LCD screen. When user interact with button, LCD backlight is switched on and remains on for 30 seconds from the last interaction. Following screens are available:

Showing current controller time and day of week.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9987.jpeg)

Showing current controller date with Summer/Winter DST.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9988.jpeg)

Showing current position of clock indicators on tower.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9989.jpeg)

Display relay open state duration in milliseconds. 
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9990.jpeg)

Display motor rotating wait time in milliseconds
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0017.jpeg)

Display current temperature from Real Time Clock module. 
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9991.jpeg)

Display minimum recorded temperature and time when recorded. 
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9992.jpeg)

Display maximum recorded temperature and time when recorded. 
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9993.jpeg)

Mains power status display. Possible values Ok or Fail. In the Fail state, the relay is not triggered and waits until power is restored.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9994.jpeg)

Display last mains power failure date and time
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0018.jpeg)

Display total uptime in seconds
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0019.jpeg)

Display total downtime in seconds
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0020.jpeg)

Display last time any setting or clock has been adjusted
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0024.jpeg)

Display last time controller was reset or startup. If RTC time was never configured before then this time can be wrong.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0025.jpeg)

## **Components**

### Controller Arduino Nano
![image](https://github.com/avgustint/tower-clock/assets/9412797/8fbae44a-66fd-4745-98ee-94f49e7eb06a)

### LCD display LCM1602C
![image](https://github.com/avgustint/tower-clock/assets/9412797/59cefcb1-090a-4a67-a93b-77a3ea650e69)

### Rechargeable Lithium Battery LP103395 3.7V 3700mAh 13.7Wh
![image](https://github.com/avgustint/tower-clock/assets/9412797/0f3e59af-5f53-4cb3-9975-40ea88144ab4)

### Power Supply and Charging Controller
![image](https://github.com/avgustint/tower-clock/assets/9412797/9fa9ace6-a631-413e-a117-079d3b30c8a3)

### Real Time Clock Module DS3231
![image](https://github.com/avgustint/tower-clock/assets/9412797/990adb97-49cd-43ab-b6b3-29cf0235794e)

### Rotary Encoder with Press Button
![image](https://github.com/avgustint/tower-clock/assets/9412797/0cd3fe18-62a0-431a-86cb-1cd6314a8132)

### Relay Module
![image](https://github.com/avgustint/tower-clock/assets/9412797/2f2f898b-8374-46ce-9868-71399b85e3fd)

## **Tower Clock Motor** 

Few images from tower clock motor that rotates the clock indicators. Motor has also own switch that remains open until indicators not rotated for one minute. This is the reason we use additional motor delay in configuration.

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

* Replace mechanical relay module with Mosfet
* Add Lora remote diagnostic and configuration options. Option to remote setup exact time from NTP server.


March 2024 
