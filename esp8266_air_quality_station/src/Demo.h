#include "SdsDustSensorResults.h"

class SdsDustSensor {
public:
    int lastIdx = 0;
    float limit = 10;
    float sampleValues[10];

    SdsDustSensor(int pinA, int pinB, float limit = 10.0) {
        int tenpercent = limit * 0.1;
        int belowlimit = limit - 1;
        sampleValues[0] = belowlimit;
        sampleValues[1] = limit; 
        sampleValues[2] = limit;
        sampleValues[3] = limit + tenpercent - 0.1;
        sampleValues[4] = limit + tenpercent;
        sampleValues[5] = limit + tenpercent + 0.1;
        sampleValues[6] = belowlimit;
        sampleValues[7] = belowlimit;
        sampleValues[8] = belowlimit;
        sampleValues[9] = belowlimit;
    }

    void begin(int baudRate = 9600) { }
    
    PmResult queryPm() {
        PmResult res =  PmResult(Status::Ok, response);
        res.pm10 = random(100, 999) / 10.0;
        res.pm25 = random(100, 999) / 10.0;

        // res.pm10 = res.pm25 = sampleValues[lastIdx];
        // lastIdx = lastIdx < 9 ? lastIdx+1 : 0;

        return res;
    }

    WorkingStateResult wakeup() {
        return WorkingStateResult(Status::Ok, response);
    }
   
    ReportingModeResult setQueryReportingMode() {
        return ReportingModeResult(Status::Ok, response);
    }

    WorkingStateResult sleep() {
        return WorkingStateResult(Status::Ok, response);
    }

    byte response[Result::lenght];
};