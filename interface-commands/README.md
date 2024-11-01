
Commands on tcp interface
==========================

The query string interpreted by smtherm consists of key value pairs, where
the key and value separated by : and all pairs closed by ;
All command sends a json answer.

Main commands, and samples
--------------------------

QTT - Query target temperature, and thermostat state

    cmd:qtt;

STT - Set target temperature

    cmd:stt;ttemp:<TARGET-TEMP>;

STW - Set thermostat working

    cmd:stw;work:<on|off>;

QAS - Query all sensors

    cmd:qas;

QHIS - Query history of one sensor

    cmd:qhis;sn:<SENSOR-NAME>;off:<OFFSET>;len:<LENGTH>;
    cmd:qhis;sn:kitchen;off:0;len:720;

QHSHIS - Query heating state history 

    cmd:qhshis;

QSTAT - Query sensor statistics

    cmd:qstat;sn:<SENSOR-NAME>;

