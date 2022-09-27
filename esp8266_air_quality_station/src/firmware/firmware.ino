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
#include <AsyncElegantOTA.h>
#include "LinkedList.h"
#include "Sensors.h"
/*****************************REMOTEDEBUG****************************************/
#include <RemoteDebug.h> //https://github.com/JoaoLopesF/RemoteDebug
RemoteDebug Debug;

// Includes which uses RemoteDebug (debugV, debugE, ...)
#include "Config.h"
#include "ShellySwitch.h"
#include "WebSocketServer.h"
#include "Version.h"

#if SENSOR_SDS
    #include "SDSSensor.h"
#else
    #include "SPS030Sensor.h"
#endif

WebSocketServer webSocketServer("/ws");

/*****************************NTP TIME*******************************************/
#define NTP_OFFSET   60 * 60      // In seconds
#define NTP_INTERVAL 60 * 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "europe.pool.ntp.org"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, 0, NTP_INTERVAL);


/*****************************CONFIG*********************************************/
Config config;

/*****************************INFLUXDB*********************************************/
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#define INFLUXDB_URL "https://europe-west1-1.gcp.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "rGVVQ7giO6M2NzUW6mhZLmUW7aOHJmz6VVYXb5mT3xs-oTijfwg7DRaYSdAd5z849Sd2oV5FFXh2Jn727BNW_g=="
#define INFLUXDB_ORG "csiszar.ati@gmail.com"
#define INFLUXDB_BUCKET "csiszar.ati's Bucket"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

Point influxData("sps030");

void setupInflux()
{
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

    // Check server connection
    if (client.validateConnection()) {
        debugV("Connected to InfluxDB.");
    } else {
        debugV("InfluxDB connection failed: %s", client.getLastErrorMessage());
    }
}

void sendToInflux(Sensors data)
{
    influxData.clearFields();

    influxData.addField("ppm1", data.pm1);
    influxData.addField("ppm2", data.pm25);
    influxData.addField("ppm4", data.pm4);
    influxData.addField("ppm10", data.pm10);
    influxData.addField("aqi", data.aqi());
    influxData.addField("at", data.at);
    influxData.addField("decision", data.switch_ai_decision);
    //influxData.setTime(data.at);
    
    if (!client.writePoint(influxData)) {
        debugE("InfluxDB write failed: %s", client.getLastErrorMessage());
    } else {
        debugV("Influxdb write success.");
    }
}

/*****************************WIFI***********************************************/
WiFiManager wifiManager;

void setupWifiManager()
{
    WiFi.hostname(config.mdns_hostname);
    // wifiManager.resetSettings();
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

Switch shelly;

/*****************************WEBSOCKET SERVER****************************************/

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
            char sensorsPayload[200];
            WebSocketMessage::createSensorsEventMessage(sensorsPayload, sensorsHistory.last());
            client->text(sensorsPayload);
        }

        if (strcmp(event, "save-settings") == 0) {
            if (config.measuring_frequency != payload["data"]["measuring_frequency"]) {
                forceStartMeasuring = true;
            }

            config.fillFromWebsocketMessage(payload);
            config.saveToFile();

            shelly.setIP(config.shelly_ip);

            char infoMessagePayload[100];
            WebSocketMessage::createInfoEventMessage(infoMessagePayload, "settings.saved");
            client->text(infoMessagePayload);

            webSocketServer.notifyClientsWithConfig(config);
        }

        if (strcmp(event, "measure-aqi") == 0) {
            forceStartMeasuring = true;
        }

        if (strcmp(event, "restart") == 0) {
           ESP.restart();
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
        
        char configPayload[300];
        WebSocketMessage::createConfigEventMessage(config, configPayload);
        client->text(configPayload);
        
        measuring
            ? webSocketServer.notifyClientsThatMeasuringStarted(nextReadAt)
            : webSocketServer.notifyClientsAboutNextWakeUp(wakeUpAt);

        for (int idx = 0; idx < sensorsHistory.items.size(); idx++) {
            char sensorsPayload[200];
            WebSocketMessage::createSensorsEventMessage(sensorsPayload, sensorsHistory.items.get(idx));
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
      
    httpServer.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/config.json", "application/json");
        request->send(response);
    });

    httpServer.on("/sensors.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("text/plain");
        sensorsHistory.printToResponse(response);
        request->send(response);
    });

    webSocketServer.onEvent(onWebsocketEvent);

    httpServer.addHandler(&webSocketServer);

    AsyncElegantOTA.begin(&httpServer);

    httpServer.begin();
}

volatile unsigned long nextGoogleSheetsUpdateAt = 0;

#if (SDS_SENSOR)
    #define GOOGLE_SHEET_UPDATE_INTERVAL 5*60000 // 5m
#else
    #define GOOGLE_SHEET_UPDATE_INTERVAL 30000 // 30sec
#endif

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

    char parameters[300];

    Sensors sensors = sensorsHistory.last();
    
    snprintf(
        parameters,
        250,
        "?temperature=%.1f&humidity=%.1f&pm25=%.2f&pm10=%.2f&pm4=%.2f&pm1=%.2f&aqi=%d&ai=%s&uuid=%s",
        sensors.temp,
        sensors.humidity,
        sensors.pm25,
        sensors.pm10,
        sensors.pm4,
        sensors.pm1,
        sensors.aqi(),
        sensors.switch_ai_decision == ABOVE ? "Felette\0"
        : sensors.switch_ai_decision == BELOW ? "Alatta\0"
        : sensors.switch_ai_decision == SWITCH_OFF ? "Ki\0"
        : sensors.switch_ai_decision == SWITCH_ON ? "Be\0"
        : sensors.switch_ai_decision == BEFORE_SWITCH_ON_TIME ? "Alatta, de ido elott\0"
        : "-\0",
        UUID
    );

#if SENSOR_SDS
    client->write("GET /macros/s/AKfycbypJ1kblXzkFC05FG_OlAuAoghtzLHWvQxKG7s1MGFpCPblUSbL6iT_VQ/exec");
#else
    client->write("GET /macros/s/AKfycbyGuDNW_v6MFAqGj6S2ujxHutmZ5XFG_oxodKUnGg/exec");
#endif
    client->write(parameters);
    client->write(" HTTP/1.1\r\nHost: script.google.com\r\n\r\n");
    client->flush();
}

/*****************************MAIN*********************************************/

volatile unsigned long notifyClientsWithSensorsDataAt = 0;

#define WEBSOCKET_SENSOR_DATA_UPDATE_INTERVAL 30000; // 30 s

unsigned int _lastSwitchOffAt = 0;

unsigned int switchOffDecisions = 0;
unsigned int switchOnDecisions = 0;

void switchShellyBySensorData(Sensors* data, int ppm_limit)
{
    if (data->aqi() >= ppm_limit) {
        data->switch_ai_decision = ABOVE;
        switchOnDecisions = 0;
        
        if (++switchOffDecisions >= config.required_switch_decisions) {
            data->switch_ai_decision = SWITCH_OFF;
            switchOffDecisions = 0;
            _lastSwitchOffAt = millis();
            shelly.turnOffPendingStateChange();
            shelly.turnOff();
        }
    } else {
        data->switch_ai_decision = BELOW;
        switchOffDecisions = 0;
        switchOnDecisions++;

        if (_lastSwitchOffAt
            && (_lastSwitchOffAt + config.switch_back_time*60*1000) > millis() // switch_back_time x minutes
        ) {
            data->switch_ai_decision = BEFORE_SWITCH_ON_TIME;
            return;
        }

        if (switchOnDecisions >= config.required_switch_decisions) {
            data->switch_ai_decision = SWITCH_ON;
            switchOnDecisions = 0;
            _lastSwitchOffAt = 0;
            shelly.turnOffPendingStateChange();
            shelly.turnOn();
        }
    }
}

void refreshSensors()
{
    shelly.refreshState();

    Sensors data;

    // readTemperatureSensor();

    if (readAirQualitySensor(&data)) {
        data.at = timeClient.getEpochTime();
        
        if (config.auto_switch_enabled) {    
            switchShellyBySensorData(&data, config.ppm_limit);
        }

        sensorsHistory.addData(data);
        // sensorsHistory.print();
        notifyClientsWithSensorsDataAt = 0;

        webSocketServer.notifyClientsAboutNextWakeUp(wakeUpAt);

        sendToInflux(data);
    }

    if (! sensorsHistory.isEmpty()
        && notifyClientsWithSensorsDataAt < millis()
    ) {
        webSocketServer.notifyClientsWithSensorsData(sensorsHistory.last());
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

    config.loadFromFile();

    setupWifiManager();

    setupHttpServer();

    setupMDNS();

    setupInflux();

    shelly.setIP(config.shelly_ip);

    shelly.refreshState();

    tempSensor.begin();
    
    setupAirQualitySensor();

    nextGoogleSheetsUpdateAt = millis() + GOOGLE_SHEET_UPDATE_INTERVAL;

    sensorsHistory.restore();
}

void loop() {
    MDNS.update();

    Debug.handle();

    timeClient.update();

    refreshSensors();

    shelly.handleStateSwitch();
    
    sendDataToGoogleSheets();
}
