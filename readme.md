# **Tower Clock Arduino Controller**

## **Introduction**

Controlling the clock in a church tower with an arduino microcontroller using a real time clock (RTC) module that stores and returns the correct time. The controller attempts to synchronise the time of the clock in the tower with the system time of the controller. Every full minute, the tower clock must be moved forward so that the AC motor is running. The motor is triggered by the opening of a relay. The tower clock shall be stopped, if there is no power in the mains to turn the motor. The clock in the tower will synchronise when we have power from the grid again.

Program is taking into consideration also the rules for Daylight Saving Time (DST) in Europe. On the last Sunday in March, time moves forward one hour from 2:00 to 3:00 and on the last Sunday in October, time moves back from 3:00 to 2:00. When the time moves forward, the tower clock will catch the correct time. When the time moves back, the tower clock will wait one hour.

The controller uses a rotary encoder with a push-button to set all the necessary variables (system time, current time of the tower clock, relay delay, etc.). All settings and configurations are displayed on an LCD screen with automatic on/off illumination.

Use of lithium battery as UPS controller. The controller is powered by a 5 V DC supply connected to the mains. In case of mains failure, the lithium battery is switched over to the lithium battery with a lithium charging module, which also takes care of charging the battery.

The RTC module is temperature compensated. The RTC module reads and records the minimum/maximum temperature and displays it on LCD screen.

## **Wiring**

Wiring diagram how to connect all required components.

![image](https://github.com/avgustint/tower-clock/assets/9412797/83a2db21-fc10-48a3-97cc-b9a19ab0a652)

## **Construction**

Connect all required components with Arduino Nano microcontroller and enclose in appropriate housing.

![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0013.jpeg)

![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0014.jpeg)

![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0015.jpeg)

![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9985.jpeg)

## **Setup**

Instruction how to setup the controller with the correct time and current tower time.

Press the rotary encoder button to enter setup mode when on date or time screen. When enterring setup mode wizard starts to enter all required values step by step. All inputed values are stored when confirming last step (relay delay). Rotate the rotary encoder button to change the values.

Step 1: Enter correct controller system hour.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9997.jpeg)

Step 2: Enter correct controller system minutes. On confirm step 0 seconds will be set.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9999.jpeg)

Step 3: Enter current day of month.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0001.jpeg)

Step 4: Enter current month.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0002.jpeg)

Step 5: Enter current year. Only last 2 digits of current century.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0003.jpeg)

Step 6: Enter day of the week (Monday, Tuesday, Wednesday,...)
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0004.jpeg)

Step 7: Enter summer or winter time regarding daylight saving time rules.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0005.jpeg)

Step 8: Enter tower clock current hour
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0006.jpeg)

Step 9: Enter tower clock current minute
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0007.jpeg)

Step 10: Enter relay opening state delay in miliseconds in step of 10 miliseconds.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_0008.jpeg)

On confirming last step, values are stored and set. New time is now used and controller returns to normal operational mode.

## **Usage**

When conttroller operates in normal mode, you can use rottary encoder button to display different information on LCD screen. When user interact with button, LCD backlight is switched on and remains on for 30 seconds from the last interaction. Following screens are available:

Showing current controller time and day of week.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9987.jpeg)

Showing current controller date with Summer/Winter DST.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9988.jpeg)

Showing current position of clock indicators on tower.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9989.jpeg)

Display relay open state duration in miliseconds. 
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9990.jpeg)

Display current temperature from Real Time Clock module. 
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9991.jpeg)

Display minimum recorded temperature and time when recorded. 
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9992.jpeg)

Display maximum recorded temperature and time when recorded. 
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9993.jpeg)

Mains power status display. Possible values Ok or Fail. In the Fail state, the relay is not triggered and waits until power is restored.
![image](https://github.com/avgustint/tower-clock/blob/main/images/controller/IMG_9994.jpeg)

## **Components**

### Controller Arduino Nano
![image](https://github.com/avgustint/tower-clock/assets/9412797/8fbae44a-66fd-4745-98ee-94f49e7eb06a)


### Rechargeable Lithium Battery 
![image](https://github.com/avgustint/tower-clock/assets/9412797/0f3e59af-5f53-4cb3-9975-40ea88144ab4)

### Power Supply and Charging Controller
![image](https://github.com/avgustint/tower-clock/assets/9412797/9fa9ace6-a631-413e-a117-079d3b30c8a3)

### Real Time Clock Module
![image](https://github.com/avgustint/tower-clock/assets/9412797/990adb97-49cd-43ab-b6b3-29cf0235794e)

### Rotary Encoder with Press Button
![image](https://github.com/avgustint/tower-clock/assets/9412797/0cd3fe18-62a0-431a-86cb-1cd6314a8132)

### Relay Module
![image](https://github.com/avgustint/tower-clock/assets/9412797/2f2f898b-8374-46ce-9868-71399b85e3fd)

## **Tower Clock Motor** 

Few images from tower clock motot that rotate the clock indicators. Motor has also own switch that remains open until indicators not rotated for one minute.

![image](https://github.com/avgustint/tower-clock/blob/main/images/motor/IMG_9879.jpeg)

![image](https://github.com/avgustint/tower-clock/blob/main/images/motor/IMG_9880.jpeg)

![image](https://github.com/avgustint/tower-clock/blob/main/images/motor/IMG_9881.jpeg)

![image](https://github.com/avgustint/tower-clock/blob/main/images/motor/IMG_9882.jpeg)

![image](https://github.com/avgustint/tower-clock/blob/main/images/motor/IMG_9883.jpeg)




March 2024 
