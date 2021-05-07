#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <WiFiClient.h>
// https://github.com/me-no-dev/ESPAsyncWebServer/issues/418#issuecomment-667976368
#define WEBSERVER_H 1
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "src/LinkedList.h"
#include "src/Sensors.h"

#ifndef DEMO_MODE
    #define DEMO_MODE 0
#endif

#if DEMO_MODE == 1
    #include "src/Demo.h"
#else
    #include <SdsDustSensor.h>
#endif


/*****************************NTP TIME*******************************************/
#define NTP_OFFSET   60 * 60      // In seconds
#define NTP_INTERVAL 60 * 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "europe.pool.ntp.org"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, 0, NTP_INTERVAL);

/*****************************REMOTEDEBUG****************************************/
#include <RemoteDebug.h> //https://github.com/JoaoLopesF/RemoteDebug

RemoteDebug Debug;

/*****************************CONFIG*********************************************/

struct Config {
    int ppm_limit = 1000;
    char shelly_ip[40];
    char mdns_hostname[50] = "LMSzenzor";
    bool auto_switch_enabled = true;
};

Config config;
const char *configFilename = "/config.json";

void loadConfiguration(const char *filename, Config &config)
{
    File configFile = LittleFS.open(configFilename, "r");
    if (!configFile) {
        debugE("Failed to open config file for reading");
        // return;
    }

    const size_t capacity = JSON_OBJECT_SIZE(2) + 40;
    DynamicJsonDocument json(capacity);

    DeserializationError error = deserializeJson(json, configFile);
    if (error) {
        debugE("Failed to read file, using default configuration. Error: %s", error.c_str());
    }

    config.ppm_limit = json["ppm_limit"] | 4321;
    
    strlcpy(
        config.shelly_ip,
        json["shelly_ip"] | "192.168.0.1",
        sizeof(config.shelly_ip)
    );

    config.auto_switch_enabled = json["auto_switch_enabled"] | true;

    configFile.close();
}

void saveConfiguration(const char *filename, const Config &config)
{
    debugV("Saving config...");

    File configFile = LittleFS.open(configFilename, "w");
    if (!configFile) {
        debugV("Failed to open config file for writing");
        return;
    }

    const size_t capacity = JSON_OBJECT_SIZE(2);
    DynamicJsonDocument json(capacity);

    json["shelly_ip"] = config.shelly_ip;
    json["ppm_limit"] = config.ppm_limit;
    json["auto_switch_enabled"] = config.auto_switch_enabled;

    if (serializeJson(json, configFile) == 0) {
        debugE("Failed to write config to file.");
    }
    
    configFile.close();
}

/*****************************WIFI***********************************************/
WiFiManager wifiManager;

void setupWifiManager()
{
    WiFi.hostname(config.mdns_hostname);
    wifiManager.autoConnect(config.mdns_hostname);
    debugV("WiFi connected. IP address: %s", WiFi.localIP().toString().c_str());
}

void setupMDNS()
{
    if (!MDNS.begin(config.mdns_hostname)) {
        debugE("MDNS: start failed");
    } else {
        MDNS.addService("http", "tcp", 80);
    }
}

/*****************************REMOTEDEBUG*************************************/
void setupDebug()
{
    Debug.begin("LMSzenzor");
    Debug.showColors(true);
    Debug.setSerialEnabled(true);
}

/*****************************Sensors*****************************************/
SensorsHistory sensorsHistory;

/*****************************Temperature Sensor*******************************/
#define DHT_PIN 4 // D2

DHT tempSensor = DHT(DHT_PIN, DHT11);

#define TEMP_SENSOR_READ_INTERVAL 5 * 60000; // 5m
volatile unsigned long nextTempReadAt = 0;

// void readTemperatureSensor()
// {
//     if (nextTempReadAt < millis()) {
//         sensors.temp = tempSensor.readTemperature();
//         sensors.humidity = tempSensor.readHumidity();
        
//         debugV("Read T: %.2f, H: %.2f", sensors.temp, sensors.humidity);

//         nextTempReadAt = millis() + TEMP_SENSOR_READ_INTERVAL;
//     }
// }

/*****************************AIR QUALITY SENSOR - SDS011*********************/
#define RX_PIN 12 // D6
#define TX_PIN 13 // D7
SdsDustSensor sdsSensor(RX_PIN, TX_PIN);

void setupAirQualitySensor()
{
    sdsSensor.begin();
    delay(10);
    sdsSensor.wakeup();
    delay(10);

    // We want to query the data from the sensor
    sdsSensor.setQueryReportingMode();
}

volatile unsigned long wakeUpAt = 0;
volatile unsigned long nextReadAt = 0;
volatile unsigned long nextMeasureAt = 0;
volatile unsigned int readAttempts = 5;

#define SDS_SENSOR_READ_INTERVAL 1 * 60000; // 1m

bool readAirQualitySensor(Sensors *sensors)
{
    if (wakeUpAt < millis()) {
        debugV("Wakeup sensor");
        sdsSensor.wakeup();
        wakeUpAt = millis() + SDS_SENSOR_READ_INTERVAL;
        nextReadAt = nextMeasureAt = millis() + 30*1000; // 30s
    }

    if (nextReadAt && nextReadAt < millis()) {
        if (readAttempts == 0) {
            debugV("Sleep sensor");
            readAttempts = 5;
            nextReadAt = 0;
            sdsSensor.sleep();
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

            return true;
        } else {
            debugV("Air Quality Sensor read error: %s", result.statusToString().c_str());
            
            if (result.status == Status::NotAvailable) {
                notifyClientsWithError("aqs.notfound");
            } else {
                char *errorCode = "aqs.error:";
                strcat(errorCode, result.statusToString().c_str());
                notifyClientsWithError(errorCode);
            }
        }
    }

    return false;
}

/*****************************Shelly Switch***********************************/
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

            HTTPClient httpClient;

            // https://arduinojson.org/v6/how-to/use-arduinojson-with-httpclient/
            // Unfortunately, by using the underlying Stream, we bypass the code that 
            // handles chunked transfer encoding, so we must switch to HTTP version 1.0.
            httpClient.useHTTP10(true);
            httpClient.begin(config.shelly_ip, 80, "/relay/0/");
            int responseCode = httpClient.GET();
            
            if (responseCode < 0) {
                debugE("Get Switch State Error: %d", responseCode);
                notifyClientsWithError("shelly.notfound");
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
        }

        void setState(RelayState newState) {
            HTTPClient httpClient;

            String path("/relay/0?turn=");
            path.concat(newState == ON ? "on" : "off");

            httpClient.begin(config.shelly_ip, 80, path);
            int responseCode = httpClient.GET();

            if (responseCode < 0) {
                debugE("Switch setState Error: %d", responseCode);
                notifyClientsWithError("shelly.setstate.failed");
            }

            httpClient.end();

            state = newState;
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

        void handleAutoSwitch()
        {
            if (! config.auto_switch_enabled) {
                return;
            }

             _changeStateNextTime = false;

            if (_nextAutoSwitchTime < millis()) {
                if (sensorsHistory.items.size() <= 0) {
                    return;
                }

                Sensors last = sensorsHistory.last();

                if (last.aqi() >= config.ppm_limit) {
                    turnOff();
                    _nextAutoSwitchTime = millis() + 60*60*1000; // 1h
                } else {
                    turnOn();
                    _nextAutoSwitchTime = millis() + 60*1000; // 1m
                }
            }
        }

    private:
        RelayState state = OFF;

        bool _changeStateNextTime = false;
        RelayState _changeStateNextTimeTo = ON;

        int _nextAutoSwitchTime = 0;

        int _nextRefreshStateTime = 0;

        String ip;
};

Switch shelly;

/*****************************WEBSOCKET SERVER****************************************/

AsyncWebSocket webSocketServer("/ws");

void handleWebSocketMessage(
    AsyncWebSocketClient *client, 
    void *arg,
    uint8_t *data,
    size_t len
) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {

        debugV("WebSocket incoming message: %s", data);

        DynamicJsonDocument payload(400);

        DeserializationError error = deserializeJson(payload, data);
        if (error) {
            debugE("WebSocket incoming data deserialization error: %s", error.c_str());
            return;
        }

        const char* event = payload["event"];
        if (strcmp(event, "set-switch-on") == 0) {
           shelly.triggerStateSwitch(Switch::ON);
        }

        if (strcmp(event, "set-switch-off") == 0) {
           shelly.triggerStateSwitch(Switch::OFF);
        }

        if (strcmp(event, "sensor-status") == 0) {
            char sensorsPayload[150];
            createSensorsEventMessage(sensorsPayload, sensorsHistory.last());
            client->text(sensorsPayload);
        }

        if (strcmp(event, "save-settings") == 0) {
            config.ppm_limit = payload["data"]["ppm_limit"];
            
            strlcpy(
                config.shelly_ip,
                payload["data"]["shelly_ip"],
                sizeof(config.shelly_ip)
            );
            
            config.auto_switch_enabled = payload["data"]["auto_switch_enabled"];

            saveConfiguration(configFilename, config);

            char infoMessagePayload[100];
            createInfoEventMessage(infoMessagePayload, "settings.saved");
            client->text(infoMessagePayload);
        }

        if (strcmp(event, "measure-aiq") == 0) {
            if (! nextReadAt) {
                wakeUpAt = millis();
            }
        }
    }
}

void onWebsocketEvent(
    AsyncWebSocket *server,
    AsyncWebSocketClient *client,
    AwsEventType type,
    void *arg,
    uint8_t *data,
    size_t len
) {
    switch (type) {
      case WS_EVT_CONNECT:
        debugV("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        
        char configPayload[150];
        createConfigEventMessage(configPayload);
        client->text(configPayload);
        
        for (int idx = 0; idx < sensorsHistory.items.size(); idx++) {
            char sensorsPayload[150];
            createSensorsEventMessage(sensorsPayload, sensorsHistory.items.get(idx));
            client->text(sensorsPayload);
        }

        break;
      case WS_EVT_DISCONNECT:
        debugV("WebSocket client #%u disconnected\n", client->id());
        break;
      case WS_EVT_DATA:
        handleWebSocketMessage(client, arg, data, len);
        break;
      case WS_EVT_PONG:
      case WS_EVT_ERROR:
        break;
  }
}

void createConfigEventMessage(char *destination)
{
    char msgTemplate[] = R"===({"event":"config", "data":{ "shelly_ip":"%s", "ppm_limit":"%d", "auto_switch_enabled": %s}})===";
    
    snprintf(
        destination,
        150,
        msgTemplate,
        config.shelly_ip,
        config.ppm_limit,
        config.auto_switch_enabled ? "true" : "false"
    );
}

void createSensorsEventMessage(char *destination, Sensors data)
{
    char msgTemplate[] = R"===({"event":"sensors", "data":{ "at":%d,"aqi":%d,"pm10":%.2f,"pm25":%.2f,"temp":"%.2f", "aiqNextReadMs":%d,"switch_state":%d }})===";
    
    snprintf(
        destination,
        150,
        msgTemplate,
        data.at,
        data.aqi(),
        data.pm10,
        data.pm25,
        data.temp,
        nextMeasureAt ? nextMeasureAt - millis() : 0,
        shelly.getState() == Switch::ON ? 1 : 0
    );
}

void createInfoEventMessage(char *destination, char* code)
{
    char msgTemplate[] = R"===({"event":"info", "data":{ "code": "%s" }})===";
    
    snprintf(destination, 100, msgTemplate, code);
}

void notifyClientsWithError(char *code)
{
    char errorPayload[100];
    char msgTemplate[] = R"===({"event":"error", "data":{ "code": "%s" }})===";
    
    snprintf(errorPayload, 100, msgTemplate, code);

    webSocketServer.textAll(errorPayload);
}

void notifyClientsWithSensorsData(Sensors data)
{
    char sensorsPayload[150];

    createSensorsEventMessage(sensorsPayload, data);

    webSocketServer.textAll(sensorsPayload);
}

/*****************************HTTP SERVER****************************************/
#include "webpage.h"

AsyncWebServer httpServer(80);

void setupHttpServer()
{
    httpServer.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404);
    });
      
    httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", WEBPAGE_HTML, WEBPAGE_HTML_SIZE);
        response->addHeader("Content-Encoding","gzip");
        request->send(response);
    });
      
    httpServer.on("/alpine.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/alpine.js.gz");
        response->addHeader("Content-Encoding","gzip");
        request->send(response);
    });
      
    httpServer.on("/app.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/app.css.gz", "text/css");
        response->addHeader("Content-Encoding","gzip");
        request->send(response);
    });

    webSocketServer.onEvent(onWebsocketEvent);

    httpServer.addHandler(&webSocketServer);

    httpServer.begin();
}

volatile unsigned long nextGoogleSheetsUpdateAt = 0;

#define GOOGLE_SHEET_UPDATE_INTERVAL 5*60000 // 5m

void sendDataToGoogleSheets()
{
    if (DEMO_MODE || nextGoogleSheetsUpdateAt > millis()) {
        return;
    }

    nextGoogleSheetsUpdateAt = millis() + GOOGLE_SHEET_UPDATE_INTERVAL;

    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

    client->setInsecure();
    client->setBufferSizes(512, 512);

    client->connect("script.google.com", 443);
    if (!client->connected()) {
        debugE("Failed to connect script.google.com");
        return;
    }

    char parameters[60];

    Sensors sensors = sensorsHistory.last();
    
    snprintf(
        parameters,
        60,
        "?temperature=%.1f&humidity=%.1f&pm25=%.2f&pm10=%.2f&aqi=%d",
        sensors.temp,
        sensors.humidity,
        sensors.pm25,
        sensors.pm10,
        sensors.aqi()
    );

    client->write("GET /macros/s/AKfycbypJ1kblXzkFC05FG_OlAuAoghtzLHWvQxKG7s1MGFpCPblUSbL6iT_VQ/exec");
    client->write(parameters);
    client->write(" HTTP/1.1\r\nHost: script.google.com\r\n\r\n");
    client->flush();
}

/*****************************MAIN*********************************************/

volatile unsigned long notifyClientsWithSensorsDataAt = 0;

#define WEBSOCKET_SENSOR_DATA_UPDATE_INTERVAL 30000; // 30 s

void refreshSensors()
{
    shelly.refreshState();

    Sensors data;

    // readTemperatureSensor();

    if (readAirQualitySensor(&data)) {
        data.at = timeClient.getEpochTime();
        sensorsHistory.addData(data);
        // sensorsHistory.print();
        notifyClientsWithSensorsDataAt = 0;
    }

    if (! sensorsHistory.isEmpty()
        && notifyClientsWithSensorsDataAt < millis()
    ) {
        notifyClientsWithSensorsData(sensorsHistory.last());
        notifyClientsWithSensorsDataAt = millis() + WEBSOCKET_SENSOR_DATA_UPDATE_INTERVAL;
    }
}

void setup() {
    Serial.begin(115200);
    if (!LittleFS.begin()) {
        Serial.println("Error mounting Filesystem");
    }
    
    timeClient.begin();

    setupDebug();

    loadConfiguration(configFilename, config);

    setupWifiManager();

    setupHttpServer();

    setupMDNS();

    shelly.refreshState();

    tempSensor.begin();
    
    setupAirQualitySensor();

    nextGoogleSheetsUpdateAt = millis() + 60000; // 1m

    sensorsHistory.restore();
}

void loop() {
    MDNS.update();

    Debug.handle();

    timeClient.update();

    refreshSensors();

    shelly.handleAutoSwitch();

    shelly.handleStateSwitch();
    
    sendDataToGoogleSheets();
}
