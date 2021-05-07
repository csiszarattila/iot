#include "SdsDustSensorResults.h"

class SdsDustSensor {
public:
    SdsDustSensor(int pinA, int pinB) { }

    void begin(int baudRate = 9600) { }
    
    PmResult queryPm() {
        PmResult res =  PmResult(Status::Ok, response);

        res.pm10 = random(100, 999) / 10.0;
        res.pm25 = random(100, 999) / 10.0;

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