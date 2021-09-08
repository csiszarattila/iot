#ifndef SHELLY_SWITCH_H
#define SHELLY_SWITCH_H

#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "WebSocketServer.h"

extern WebSocketServer webSocketServer;

class Switch
{
    public:
        enum RelayState {
            OFF,
            ON,
        };

        void refreshState()
        {
            if (_nextRefreshStateTime > millis()) {
                return;
            }

            _nextRefreshStateTime = millis() + 15*1000; // 15s
            
            WiFiClient client;
            HTTPClient httpClient;

            // https://arduinojson.org/v6/how-to/use-arduinojson-with-httpclient/
            // Unfortunately, by using the underlying Stream, we bypass the code that 
            // handles chunked transfer encoding, so we must switch to HTTP version 1.0.
            httpClient.useHTTP10(true);
            httpClient.begin(client, ip, 80, "/relay/0/");
            int responseCode = httpClient.GET();
            
            if (responseCode < 0) {
                debugE("Get Switch State Error: %d", responseCode);
                webSocketServer.notifyClientsWithError("shelly.notfound");
                httpClient.end();
                return;
            }

            DynamicJsonDocument jsonBuffer(220);
            
            DeserializationError error = deserializeJson(jsonBuffer, httpClient.getStream());
            if (error) {
                debugE("Failed to get Shelly Switch state, json deserialize error: %s", error.c_str());
            }

            bool _isOn = jsonBuffer["ison"] | false;

            debugD("Switch RefreshState result: %d", _isOn);

            state = _isOn ? ON : OFF;

            httpClient.end();

            webSocketServer.notifyClientsWithShellyData(this);
        }

        void setState(RelayState newState) {
            WiFiClient client;
            HTTPClient httpClient;

            String path("/relay/0?turn=");
            path.concat(newState == ON ? "on" : "off");

            httpClient.begin(client, ip, 80, path);
            int responseCode = httpClient.GET();

            if (responseCode < 0) {
                debugE("Switch setState Error: %d", responseCode);
                webSocketServer.notifyClientsWithError("shelly.setstate.failed");
            }

            httpClient.end();

            state = newState;

            webSocketServer.notifyClientsWithShellyData(this);
        }

        RelayState getState() {
            return state;
        }

        void turnOn() {
            if (state != ON) {
                setState(ON);
            }
        }

        void turnOff() {
            if (state != OFF) {
                setState(OFF);
            }
        }

        void toggle() {
            state == ON ? turnOff() : turnOn();
        }

        // https://github.com/me-no-dev/ESPAsyncWebServer/issues/364
        void triggerStateSwitch(RelayState state)
        {
            _changeStateNextTime = true;
            _changeStateNextTimeTo = state;
        }

        void handleStateSwitch()
        {
            if (_changeStateNextTime) {
                setState(_changeStateNextTimeTo);
                _changeStateNextTime = false;
            }
        }

        void turnOffPendingStateChange()
        {
             _changeStateNextTime = false;
        }

        void setIP(char *_ip)
        {
            ip = _ip;
        }

    private:
        RelayState state = OFF;

        bool _changeStateNextTime = false;
        RelayState _changeStateNextTimeTo = ON;

        int _nextAutoSwitchTime = 0;

        int _nextRefreshStateTime = 0;

        char *ip;
};

#endif