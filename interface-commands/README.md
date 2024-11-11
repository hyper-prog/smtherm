
Commands on tcp interface
==========================

The query string interpreted by smtherm consists of key value pairs, where
the key and value separated by : and all pairs closed by ;
All command sends a json answer.

Main commands, and samples
--------------------------

**QTT - Query target temperature, and thermostat state**

    cmd:qtt;

  Sample answer

    echo "cmd:qtt;" | netcat 127.0.0.1 5017 | jq
    {
      "working": "on",
      "target_temp": 20,
      "reference_temp": 19.9,
      "heating_state": "off"
    }


**STT - Set target temperature**

    cmd:stt;ttemp:<TARGET-TEMP>;

  Sample answer

    echo "cmd:stt;ttemp:21.0;" | netcat 127.0.0.1 5017 | jq
    {
      "set": "ok"
    }


**STW - Set thermostat working**

    cmd:stw;work:<on|off>;

  Sample answer

    echo "cmd:stw;work:on;" | netcat 127.0.0.1 5017 | jq
    {
      "set": "ok"
    }


**QAS - Query all sensors**

    cmd:qas;

  Sample answer

    echo "cmd:qas;" | netcat 127.0.0.1 5017 | jq
    {
      "sensors": [
        {
          "name": "hall",
          "temp": 20.1,
          "hum": 47
        },
        {
          "name": "bedroom",
          "temp": 19.6,
          "hum": 53
        },
        {
          "name": "garden",
          "temp": 6.1,
          "hum": 69
        },
        {
          "name": "rack",
          "temp": 25.9,
          "hum": 36
        }
      ]
    }


**QHIS - Query history of one sensor**

    cmd:qhis;sn:<SENSOR-NAME>;off:<OFFSET>;len:<LENGTH>;
    cmd:qhis;sn:kitchen;off:0;len:720;

  Sample answer

    echo "cmd:qhis;sn:kitchen;off:0;len:10;" | netcat 127.0.0.1 5017 | jq
    {
      "st": 1730753235,
      "d": [
        [588808,20.3,47],
        [589111,20.3,47],
        [589423,20.3,47],
        [589735,20.3,47],
        [590042,20.3,47],
        [590347,20.2,47],
        [590653,20.2,47],
        [590958,20.2,47],
        [591269,20.2,46],
        [591583,20.2,47],
        [591894,20.1,47]
      ]
    }


**QHSHIS - Query heating state history**

    cmd:qhshis;

  Sample answer

    echo "cmd:qhshis;" | netcat 127.0.0.1 5017 | jq
    {
      "hswhist": [
        [1730768395,1],
        [1730770111,2],
        [1730787019,1],
        [1730788615,2]
      ]
    }

  Here the 1 means heating start, 2 heating stop.

**QSTAT - Query sensor statistics**

    cmd:qstat;sn:<SENSOR-NAME>;

  Sample answer

    echo "cmd:qstat;sn:kitchen;" | netcat 127.0.0.1 5017 | jq
    {
      "name": "kitchen",
      "temp": 20.1,
      "lastok": "yes",
      "hum": 47,
      "lastread": 13,
      "okread": 9651,
      "crcerror": 3656,
      "insense": 246
    }

