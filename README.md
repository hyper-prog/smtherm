![SMTherm logo](https://raw.githubusercontent.com/hyper-prog/smtherm/master/images/smtherm_xxs.png)

SMTherm - Smart Manageable Thermostat daemon
============================================

SMTherm is a [thermostat](https://en.wikipedia.org/wiki/Thermostat) [daemon](https://en.wikipedia.org/wiki/Daemon_(computing)) written in C that monitors the ambient temperature using DHT22 sensors.
It designed to run on [Raspberry Pi](https://en.wikipedia.org/wiki/Raspberry_Pi) and can even take into account the data of several thermometers
and make complex decisions about starting or stopping the heating.
The program communicates via a TCP interface and, in addition to controlling the heating,
stores the temperature data of the last 3 days in a [ring buffer](https://en.wikipedia.org/wiki/Circular_buffer).

The SMTherm only have TCP interface, the GlowDash: https://github.com/hyper-prog/glowdash can controls it.

![SMTherm architecture](https://raw.githubusercontent.com/hyper-prog/smtherm/master/images/smthermarch.png)

Compile / Install
-----------------
Require wiringPi and CURL libraries

    $make
    #make install

Run as systemd daemon
---------------------

Create systemd start file /lib/systemd/system/smtherm.service

    [Unit]
    Description=SMTherm thermostat daemon
    After=network.target

    [Service]
    Type=forking
    ExecStart=/usr/local/bin/smtherm /etc/smtherm.conf
    Restart=always
    User=root
    Group=root

    [Install]
    WantedBy=multi-user.target

After the file created, edit a sample config file and place into /etc/smtherm.conf
Eenable and start the service

    systemctl daemon-reload
    systemctl enable smtherm.service
    systemctl start smtherm.service


Author
------
- Written by Péter Deák (C) hyper80@gmail.com, License GPLv2
- The author wrote this project entirely as a hobby. Any help is welcome!

------

[![paypal](https://raw.githubusercontent.com/hyper-prog/smtherm/master/images/tipjar.png)](https://www.paypal.com/donate/?business=EM2E9A6BZBK64&no_recurring=0&currency_code=USD) 
