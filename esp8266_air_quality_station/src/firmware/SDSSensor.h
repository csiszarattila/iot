/*****************************AIR QUALITY SENSOR - SDS011*********************/
#ifndef AQS_SDS_SENSOR_H
#define AQS_SDS_SENSOR_H

#include "Sensors.h"
#include "WebSocketServer.h"

extern WebSocketServer webSocketServer;
extern Config config;

#if DEMO_MODE == 1
    #include "SDSSensorDemo.h"
#else
    #include <SdsDustSensor.h>
#endif

#define RX_PIN 12 // D6
#define TX_PIN 13 // D7
SdsDustSensor sdsSensor(RX_PIN, TX_PIN);

volatile unsigned long wakeUpAt = 0;
volatile unsigned long nextReadAt = 0;
volatile unsigned int readAttempts = 5;
volatile boolean measuring = false;
volatile boolean forceStartMeasuring = false;

#define SDS_SENSOR_WEAKUP_READ_INTERVAL 30 * 1000 // 30s

void setupAirQualitySensor()
{
    sdsSensor.begin();
    delay(10);
    sdsSensor.wakeup();
    delay(10);

    // We want to query the data from the sensor
    sdsSensor.setQueryReportingMode();
}

bool readAirQualitySensor(Sensors *sensors)
{            
    if (forceStartMeasuring) {
        if (! measuring) {
            wakeUpAt = 0;
        }
        forceStartMeasuring = false;
    }

    if (wakeUpAt < millis()) {
        debugV("Wakeup sensor");
        sdsSensor.wakeup();
        wakeUpAt = millis() + config.measuring_frequency * 60000; // x minute
        nextReadAt = millis() + SDS_SENSOR_WEAKUP_READ_INTERVAL;
        measuring = true;

        webSocketServer.notifyClientsThatMeasuringStarted(nextReadAt);
    }

    if (nextReadAt && nextReadAt < millis()) {
        if (readAttempts == 0) {
            debugV("Sleep sensor");
            readAttempts = 5;
            nextReadAt = 0;
            sdsSensor.sleep();
            measuring = false;
            return false;
        }

        nextReadAt = millis() + 3000; // 3s per queries as sds datasheet advise
        readAttempts--;

        PmResult result = sdsSensor.queryPm();
        if (result.isOk()) {
            debugV("PM10: %.2f", result.pm10);
            debugV("PM2.5: %.2f", result.pm25);

            sensors->pm10 = result.pm10;
            sensors->pm25 = result.pm25;

            readAttempts = 0;
            measuring = false;

            return true;
        } else {
            debugV("Air Quality Sensor read error: %s", result.statusToString().c_str());
            
            if (result.status == Status::NotAvailable) {
                webSocketServer.notifyClientsWithError("aqs.notfound");
            } else {
                char *errorCode = "aqs.error:";
                strcat(errorCode, result.statusToString().c_str());
                webSocketServer.notifyClientsWithError(errorCode);
            }
        }
    }

    return false;
}

#endif