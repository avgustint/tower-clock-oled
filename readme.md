# **Tower Clock Arduino Controller**

Controlling the clock in a church tower with an arduino microcontroller using a real time clock (RTC) module that stores and returns the correct time. The controller attempts to synchronise the time of the clock in the tower with the system time of the controller. Every full minute, the tower clock must be moved forward so that the AC motor is running. The motor is triggered by the opening of a relay. The tower clock shall be stopped, if there is no power in the mains to turn the motor. The clock in the tower will synchronise when we have power from the grid again.

Program is taking into consideration also the rules for Daylight Saving Time (DST) in Europe. On the last Sunday in March, time moves forward one hour from 2:00 to 3:00 and on the last Sunday in October, time moves back from 3:00 to 2:00. When the time moves forward, the tower clock will catch the correct time. When the time moves back, the tower clock will wait one hour.

The controller uses a rotary encoder with a push-button to set all the necessary variables (system time, current time of the tower clock, relay delay, etc.). All settings and configurations are displayed on an LCD screen with automatic on/off illumination.

Use of lithium battery as UPS controller. The controller is powered by a 5 V DC supply connected to the mains. In case of mains failure, the lithium battery is switched over to the lithium battery with a lithium charging module, which also takes care of charging the battery.

The RTC module is temperature compensated. The RTC module reads and records the minimum/maximum temperature and displays it on LCD screen.

March 2024 