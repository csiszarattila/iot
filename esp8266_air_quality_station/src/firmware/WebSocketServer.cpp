#include <AsyncWebSocket.h>
#include "Config.h"
#include "Sensors.h"
#include "ShellySwitch.h"
#include "Version.h"

void WebSocketServer::notifyClientsWithError(char *code)
{
    char errorPayload[100];
    char msgTemplate[] = R"===({"event":"error", "data":{ "code": "%s" }})===";
    
    snprintf(errorPayload, 100, msgTemplate, code);

    textAll(errorPayload);
}

void WebSocketServer::notifyClientsWithSensorsData(Sensors data)
{
    char sensorsPayload[200];

    WebSocketMessage::createSensorsEventMessage(sensorsPayload, data);

    textAll(sensorsPayload);
}

void WebSocketServer::notifyClientsWithShellyData(Switch *shelly)
{
    char sensorsPayload[200];

    WebSocketMessage::createShellySwitchStatusEventMessage(sensorsPayload, shelly);

    textAll(sensorsPayload);
}


void WebSocketServer::notifyClientsWithConfig(Config config)
{
    char configPayload[300];

    WebSocketMessage::createConfigEventMessage(config, configPayload);

    textAll(configPayload);
}

void WebSocketServer::notifyClientsThatMeasuringStarted(unsigned long nextReadAt)
{
    char payload[100];
    char msgTemplate[] = R"===({"event":"measuring","data":{ "nextReadAt":%d }})===";
    
    snprintf(
        payload,
        100,
        msgTemplate,
        nextReadAt - millis()
    );

    textAll(payload);
}

void WebSocketServer::notifyClientsAboutNextWakeUp(unsigned long wakeUpAt)
{
    char payload[100];
    char msgTemplate[] = R"===({"event":"sleeping","data":{ "nextWakeupAt":%d }})===";
    
    snprintf(
        payload,
        100,
        msgTemplate,
        wakeUpAt - millis()
    );

    textAll(payload);
}

void WebSocketMessage::createConfigEventMessage(Config config, char *destination)
{
    char msgTemplate[] = R"===({"event":"config", "data":{ "shelly_ip":"%s", "ppm_limit":"%d", "auto_switch_enabled": %s, "measuring_frequency": %d, "switch_back_time": %d, "required_switch_decisions": %d, "version": "%s", "aqi_sensor_type": "%s", "demo_mode": %s}})===";
    
    snprintf(
        destination,
        300,
        msgTemplate,
        config.shelly_ip,
        config.ppm_limit,
        config.auto_switch_enabled ? "true" : "false",
        config.measuring_frequency,
        config.switch_back_time,
        config.required_switch_decisions,
        AQS_SW_VERSION,
        SENSOR_SDS ? "sds" : "sps030",
        DEMO_MODE ? "true" : "false"
    );
}

void WebSocketMessage::createSensorsEventMessage(char *destination, Sensors data)
{
    char msgTemplate[] = R"===({"event":"sensors", "data":{ "at":%d,"aqi":%d,"pm1":%.2f,"pm25":%.2f,"pm4":%.2f,"pm10":%.2f,"temp":"%.2f" }})===";
    
    snprintf(
        destination,
        200,
        msgTemplate,
        data.at,
        data.aqi(),
        data.pm1,
        data.pm25,
        data.pm4,
        data.pm10,
        data.temp
    );
}


void WebSocketMessage::createShellySwitchStatusEventMessage(char *destination, Switch *shelly)
{
    char msgTemplate[] = R"===({"event":"shelly", "data":{ "state":%d }})===";
    
    snprintf(
        destination,
        50,
        msgTemplate,
        shelly->getState() == Switch::ON ? 1 : 0
    );
}

void WebSocketMessage::createInfoEventMessage(char *destination, char* code)
{
    char msgTemplate[] = R"===({"event":"info", "data":{ "code": "%s" }})===";
    
    snprintf(destination, 100, msgTemplate, code);
}