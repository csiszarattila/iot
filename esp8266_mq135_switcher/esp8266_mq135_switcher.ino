#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <WiFiClient.h>
// https://github.com/me-no-dev/ESPAsyncWebServer/issues/418#issuecomment-667976368
#define WEBSERVER_H 1
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <WiFiClientSecureBearSSL.h>
#include <SdsDustSensor.h>

/*****************************REMOTEDEBUG****************************************/
#include <RemoteDebug.h> //https://github.com/JoaoLopesF/RemoteDebug

RemoteDebug Debug;

/*****************************CONFIG*********************************************/

struct Config {
    int ppm_limit = 1000;
    char shelly_ip[40];
    char mdns_hostname[50] = "co2";
    bool auto_switch_enabled = true;
};

Config config;
const char *configFilename = "/config.json";

void loadConfiguration(const char *filename, Config &config)
{
    File configFile = SPIFFS.open(configFilename, "r");
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

    File configFile = SPIFFS.open(configFilename, "w");
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
    Debug.begin("co2");
    Debug.showColors(true);
    Debug.setSerialEnabled(true);
}

/*****************************Sensors*****************************************/
struct Sensors {
    int aqi = 0;
    float pm10 = 0;
    float pm25 = 0;
    float temp = 0;
    float humidity = 0;
};

Sensors sensors;


/*****************************Temperature Sensor*******************************/
#define DHT_PIN 4 // D2

DHT tempSensor = DHT(DHT_PIN, DHT11);

void readTemperatureSensor()
{
    sensors.temp = tempSensor.readTemperature();

    debugV("Read temperature sensor: %.2f", sensors.temp);
}
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
volatile unsigned int readAttempts = 5;

#define SDS_SENSOR_READ_INTERVAL 5 * 60 * 1000; // 5m

void readAirQualitySensor()
{
    if (wakeUpAt < millis()) {
        debugV("Wakeup sensor");
        sdsSensor.wakeup();
        wakeUpAt = millis() + SDS_SENSOR_READ_INTERVAL;
        nextReadAt = millis() + 30*1000; // 30s
    }

    if (nextReadAt && nextReadAt < millis()) {
        if (readAttempts == 0) {
            debugV("Sleep sensor");
            readAttempts = 5;
            nextReadAt = 0;
            sdsSensor.sleep();
            return;
        }

        nextReadAt = millis() + 3000; // 3s per queries as sds datasheet advise
        readAttempts--;

        PmResult result = sdsSensor.queryPm();
        if (result.isOk()) {
            debugV("PM10: %.2f", result.pm10);
            debugV("PM2.5: %.2f", result.pm25);

            sensors.pm10 = result.pm10;
            sensors.pm25 = result.pm25;
            readAttempts = 0;
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
            setState(ON);
        }

        void turnOff() {
            setState(OFF);
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

    private:
        RelayState state = OFF;

        bool _changeStateNextTime = false;
        RelayState _changeStateNextTimeTo = ON;

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
            createSensorsEventMessage(sensorsPayload);
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
        
        char sensorsPayload[150];
        createSensorsEventMessage(sensorsPayload);
        client->text(sensorsPayload);

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

void createSensorsEventMessage(char *destination)
{
    char msgTemplate[] = R"===({"event":"sensors", "data":{ "aqi":%d, "pm10": %.2f, "pm25": %.2f, "temp": "%.2f", "switch_state": %d }})===";
    
    snprintf(
        destination,
        150,
        msgTemplate,
        sensors.aqi,
        sensors.pm10,
        sensors.pm25,
        sensors.temp,
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

void notifyClientsWithSensorsData()
{
    char sensorsPayload[150];

    createSensorsEventMessage(sensorsPayload);

    webSocketServer.textAll(sensorsPayload);
}

/*****************************HTTP SERVER****************************************/

const char INDEX_HTML[] PROGMEM = R"=="==(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/5.0.0-alpha1/css/bootstrap.min.css" integrity="sha384-r4NyP46KrjDleawBgD5tp8Y7UzmLA05oM1iAEQ17CSuDqnUK2+k9luXQOfXJCJ4I" crossorigin="anonymous">
    <script src="https://cdn.jsdelivr.net/npm/popper.js@1.16.0/dist/umd/popper.min.js" integrity="sha384-Q6E9RHvbIyZFJoft+2mJbHaEWldlvI9IOYy5n3zV9zzTtmI3UksdQRVvoxMfooAo" crossorigin="anonymous"></script>
    <script src="https://stackpath.bootstrapcdn.com/bootstrap/5.0.0-alpha1/js/bootstrap.min.js" integrity="sha384-oesi62hOLfzrys4LxRF63OJCXdXDipiYWBnvTl9Y9/TRlw5xlKIEHpNyvvDShgf/" crossorigin="anonymous"></script>
    <link href="https://fonts.googleapis.com/icon?family=Material+Icons" rel="stylesheet">
    <title>CO2 Szenzor</title>
</head>
<body>
<div class="container">
    <h3><i class="material-icons md-48">sensors</i></span>Kapcsoló</h1>
    <form action="/switch/state" method="POST">
        <div class="form-check form-switch mb-3">
            <input class="form-check-input" type="checkbox" id="switch_on" name="switch_on" value="1" %SWITCH_ON% onchange="this.form.submit()">
            <label class="form-check-label" for="switch_on">Bekapcsolva</label>
        </div>
    </form>
    <h3><i class="material-icons md-48">sensors</i></span>CO<sub>2</sub> szenzor</h1>
    <div>
        <p>Jelenlegi érték (ppm): <strong>%PPM%</strong></p>
    </div>
    <h3 class="mt-3"><i class="material-icons md-48">settings</i></span>Beállítások</h1>
    <form action="/config" method="POST">
        <div class="form-check form-switch mb-3">
            <input class="form-check-input" type="checkbox" id="flexSwitchCheckDefault" %ENABLED_ON%>
            <label class="form-check-label" for="flexSwitchCheckDefault">Automatikus ki/bekapcsolás</label>
        </div>
        <div class="mb-3">
            <label for="ppm_limit" class="form-label">Bekapcsolási határ (ppm):</label>
            <strong><output name="ppm_limit_value">%PPM_LIMIT%</output></strong>
            <input type="range" class="form-range" value="1500" min="0" max="5000" step="10" id="ppm_limit" name="ppm_limit" value="%PPM_LIMIT%" oninput="this.form.ppm_limit_value.value=this.value">
        </div>
        <div class="mb-3">
            <label for="shelly_ip" class="form-label">Shelly kapcsoló ip címe:</label>
            <input type="text" class="form-control" name="shelly_ip" value="%SHELLY_IP%">
        </div>
        <button type="submit" class="btn btn-primary">Beállítások mentése</button>
    </form>
</div>
</body>
</html>
)=="==";

AsyncWebServer httpServer(80);

void setupHttpServer()
{
    httpServer.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404);
    });
      
    httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });

    webSocketServer.onEvent(onWebsocketEvent);

    httpServer.addHandler(&webSocketServer);

    httpServer.begin();
}

volatile unsigned long nextGoogleSheetsUpdateAt = 0;

#define GOOGLE_SHEET_UPDATE_INTERVAL 5*60*1000 // 5m

void sendDataToGoogleSheets()
{
    if (nextGoogleSheetsUpdateAt > millis()) {
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
    
    snprintf(
        parameters,
        60,
        "?temperature=%.1f&humidity=%.1f&pm25=%.2f&pm10=%.2f",
        sensors.temp,
        sensors.humidity,
        sensors.pm25,
        sensors.pm10
    );

    client->write("GET /macros/s/AKfycbypJ1kblXzkFC05FG_OlAuAoghtzLHWvQxKG7s1MGFpCPblUSbL6iT_VQ/exec");
    client->write(parameters);
    client->write(" HTTP/1.1\r\nHost: script.google.com\r\n\r\n");
    client->flush();
}

/*****************************MAIN*********************************************/

volatile unsigned long notifyClientsWithSensorsDataAt = 0;

#define WEBSOCKET_SENSOR_DATA_UPDATE_INTERVAL 30*1000; // 30 s

void refreshSensors()
{
    shelly.refreshState();

    readTemperatureSensor();

    readAirQualitySensor();

    if (notifyClientsWithSensorsDataAt < millis()) {
        notifyClientsWithSensorsData();
        notifyClientsWithSensorsDataAt = millis() + WEBSOCKET_SENSOR_DATA_UPDATE_INTERVAL;
    }
}

void setup() {
    Serial.begin(115200);
    if (!SPIFFS.begin()) {
        Serial.println("Error mounting Filesystem");
    }

    setupDebug();

    loadConfiguration(configFilename, config);

    setupWifiManager();

    setupHttpServer();

    setupMDNS();
    
    //setupMQSensor();

    shelly.refreshState();

    tempSensor.begin();
    
    setupAirQualitySensor();
    nextGoogleSheetsUpdateAt = millis() + GOOGLE_SHEET_UPDATE_INTERVAL;
}

void loop() {
    MDNS.update();

    Debug.handle();

    refreshSensors();

    shelly.handleStateSwitch();

    delay(30*1000);
    sendDataToGoogleSheets();
}
