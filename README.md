![SMTherm logo](https://raw.githubusercontent.com/hyper-prog/smtherm/master/images/smtherm_xxs.png)

SMTherm - Smart Manageable Thermostat daemon
============================================

SMTherm is a thermostat daemon written in C that monitors the ambient temperature using DHT22 sensors.
It designed to run on RaspberryPi and can even take into account the data of several thermometers
and make complex decisions about starting or stopping the heating.
The program communicates via a TCP interface and, in addition to controlling the heating,
stores the temperature data of the last 3 days in a ring buffer.

The SMTherm only have TCP interface, the GlowDash: https://github.com/hyper-prog/glowdash can controls it. 

Compile / Install
-----------------
Require wiringPi and CURL libraries

    $make
    #make install


Author
------
- Written by Péter Deák (C) hyper80@gmail.com, License GPLv2
- The author wrote this project entirely as a hobby. Any help is welcome!

------

[![paypal](https://raw.githubusercontent.com/hyper-prog/smtherm/master/images/tipjar.png)](https://www.paypal.com/donate/?business=EM2E9A6BZBK64&no_recurring=0&currency_code=USD) 
