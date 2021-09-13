/*****************************AIR QUALITY SENSOR - SPS030*********************/
#ifndef AQS_SPS030_SENSOR_H
#define AQS_SPS030_SENSOR_H

#include "Sensors.h"
#include "WebSocketServer.h"

extern WebSocketServer webSocketServer;
extern Config config;

#if DEMO_MODE == 1

    struct sps30_measurement {
        float mc_1p0;
        float mc_2p5;
        float mc_4p0;
        float mc_10p0;
    };

    int lastIdx = 0;
    float limit = 10.0;
    int tenpercent = limit * 0.1;
    int belowlimit = limit - 1;
    float spsSamples[10] = { 
        belowlimit,
        limit,
        limit,
        limit + tenpercent - 0.1,
        limit + tenpercent,
        limit + tenpercent + 0.1,
        belowlimit,
        belowlimit,
        belowlimit,
        belowlimit,
    };

    void sensirion_i2c_init() { }
    int16_t sps30_probe() { return 0; }
    int16_t sps30_set_fan_auto_cleaning_interval_days(int8_t days) { return 0; }
    int16_t sps30_start_measurement() { }
    int16_t sps30_read_data_ready(uint16_t* data_ready) { *data_ready = 1; return 0; }
    int16_t sps30_read_measurement(struct sps30_measurement* measurement)
    {
        // measurement->mc_1p0 = random(100, 999) / 10.0;
        // measurement->mc_2p5 = random(100, 999) / 10.0;
        // measurement->mc_4p0 = random(100, 999) / 10.0;
        // measurement->mc_10p0 = random(100, 999) / 10.0;

        measurement->mc_1p0 
            = measurement->mc_2p5 
            = measurement->mc_4p0
            = measurement->mc_10p0
            = spsSamples[lastIdx];
        lastIdx = lastIdx < 9 ? lastIdx+1 : 0;
    }
#else
    #include <sps30.h>
#endif

volatile unsigned long wakeUpAt = 0;
volatile unsigned long nextReadAt = 0;
volatile boolean measuring = false;
volatile boolean forceStartMeasuring = false;

#define SPS_SENSOR_READ_INTERVAL 10 * 1000 // 10s

void setupAirQualitySensor()
{
    sensirion_i2c_init();

    while (sps30_probe() != 0) {
        debugD("SPS sensor probing failed\n");
        delay(500);
    }

    sps30_set_fan_auto_cleaning_interval_days(7);

    sps30_start_measurement();
}

bool readAirQualitySensor(Sensors *sensors)
{
    struct sps30_measurement measurement;
    uint16_t data_ready;
    int16_t dataReadyFlag;

    if (wakeUpAt > millis()) {
        return false;
    }
    
    if (!measuring) {
        measuring = true;
        webSocketServer.notifyClientsThatMeasuringStarted(millis());
    }

    if (nextReadAt < millis()) {
        dataReadyFlag = sps30_read_data_ready(&data_ready);
        if (dataReadyFlag < 0) {
            char errorCode[50];
            snprintf(errorCode, 50, "aqs.error: %d", dataReadyFlag);
            webSocketServer.notifyClientsWithError(errorCode);
            nextReadAt = millis() + 100;
            return false;
        } else if (!data_ready) {
            debugV("data not ready");
            nextReadAt = millis() + 100;
            return false;
        } else {
            wakeUpAt = millis() + SPS_SENSOR_READ_INTERVAL;
            nextReadAt = 0;
            measuring = false;
            if (sps30_read_measurement(&measurement) < 0) {
                debugV("error reading measurement");
            } else {
                sensors->pm1 = measurement.mc_1p0;
                sensors->pm25 = measurement.mc_2p5;
                sensors->pm4 = measurement.mc_4p0;
                sensors->pm10 = measurement.mc_10p0;                
                
                return true;
            }
        }
    }
}

#endif