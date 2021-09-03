#ifndef AQS_WEBSOCKET_SERVER_H
#define AQS_WEBSOCKET_SERVER_H

#include <AsyncWebSocket.h>
#include "Sensors.h"
#include "ShellySwitch.h"
#include "Version.h"

class Switch;

class WebSocketMessage
{
    public:
        static void createConfigEventMessage(Config config, char *destination);
        static void createSensorsEventMessage(char *destination, Sensors data);
        static void createShellySwitchStatusEventMessage(char *destination, Switch *shelly);
        static void createInfoEventMessage(char *destination, char* code);
};

class WebSocketServer : public AsyncWebSocket
{
    public:
    WebSocketServer(const String& url) :AsyncWebSocket(url) { }

    void notifyClientsWithError(char *code);

    void notifyClientsWithSensorsData(Sensors data);
    
    void notifyClientsWithShellyData(Switch *shelly);
    
    void notifyClientsThatMeasuringStarted(unsigned long nextReadAt);
    
    void notifyClientsAboutNextWakeUp(unsigned long wakeUpAt);
    
};

#endif