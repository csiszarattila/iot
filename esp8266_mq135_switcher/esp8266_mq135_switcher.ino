#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>

/*****************************REMOTEDEBUG****************************************/
#include <RemoteDebug.h> //https://github.com/JoaoLopesF/RemoteDebug

RemoteDebug Debug;

/*****************************CONFIG*********************************************/
#include <FS.h>
#include <ArduinoJson.h>

struct Config {
    int ppm_limit = 1000;
    char shelly_ip[40];
    char mdns_hostname[50] = "co2";
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

    if (serializeJson(json, configFile) == 0) {
        debugE("Failed to write config to file.");
    }
    
    configFile.close();
}

/*****************************REMOTEDEBUG****************************************/
void setupDebug()
{
    Debug.begin("co2");
    Debug.showColors(true);
    Debug.setSerialEnabled(true);
};

/*****************************WIFI***********************************************/
#include <DNSServer.h>
#include <WiFiManager.h>
#include <WiFiClient.h>

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

/*****************************Shelly Switch**************************************/
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

#include <ESP8266WiFi.h>
// https://github.com/me-no-dev/ESPAsyncWebServer/issues/418#issuecomment-667976368
#define WEBSERVER_H 1
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer httpServer(80);

String processor(const String& var)
{
    if (var == "SHELLY_IP") {
        return config.shelly_ip;
    }
    if (var == "PPM_LIMIT") {
        return String(config.ppm_limit);
    }
    if (var == "SWITCH_ON") {
        return String(shelly.getState() == Switch::ON ? "checked" : "");
    }
    if (var == "ENABLED_ON") {
         return String(true ? "checked" : "");
    }
    if (var == "PPM") {
        return String("AAAAA");
    }
    return String();
}

void setupHttpServer()
{
    httpServer.onNotFound([](AsyncWebServerRequest *request)
    {
        request->send(404);
    });
      
    httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", INDEX_HTML, processor);
    });

    httpServer.on("/config", HTTP_POST, [](AsyncWebServerRequest *request) {
        config.ppm_limit = request->getParam("ppm_limit", true)->value().toInt();
        request->getParam("shelly_ip", true)->value().toCharArray(config.shelly_ip, sizeof(config.shelly_ip));

        saveConfiguration(configFilename, config);

        request->redirect("/");
    });

    httpServer.on("/switch/state", HTTP_POST, [](AsyncWebServerRequest *request) {
    
        shelly.triggerStateSwitch(
            request->hasParam("switch_on", true) ? Switch::ON : Switch::OFF
        );

        request->redirect("/");
    });

    httpServer.begin();
}

/*****************************MQ-135 Sensor************************************/
#include <MQUnifiedsensor.h>

#define Board "ESP8266"
#define Voltage_Resolution 3
#define ADC_Bit_Resolution 10
#define MQSensorType "MQ-135"
#define MQSensorPin A0
#define MQLoadResistor 10 // KOhm
#define MQRatioOnCleanAir 4.4 // RS / R0 = 4.4 ppm
#define MQRegressionMethod 2 //_PPM =  a*ratio^b

MQUnifiedsensor MQ135(Board, Voltage_Resolution, ADC_Bit_Resolution, MQSensorPin, MQSensorType);

void setupMQSensor()
{
    MQ135.setA(110.47); MQ135.setB(-2.862); // Configurate the equation values to get CO2 concentration
    MQ135.setRL(MQLoadResistor);
    MQ135.setRegressionMethod(MQRegressionMethod); 
    MQ135.init();
    calibrateMQSensor();
}

int calibrateMQSensor()
{
    Serial.print("MQ135: Calibrating please wait.");
    float calculatedR0 = 0;
    int numberOfSamples = 100;
    for (int i=1; i <= numberOfSamples; i++) {
        MQ135.update();
        calculatedR0 += MQ135.calibrate(MQRatioOnCleanAir);
        Serial.print(".");
    }

    MQ135.setR0(calculatedR0 / numberOfSamples);
    Serial.println("  done!.");

    Serial.print("MQ135: Calibrated R0  = ");
    Serial.println(calculatedR0 / numberOfSamples);

    if (isinf(calculatedR0) || calculatedR0 == 0) {
        Serial.println("MQ135: Error, R0 value invalid, please check your wiring and supply");
        while(1);
    }

    MQ135.serialDebug(true);
}

int readMQSensor()
{
    MQ135.update();
    MQ135.readSensor();
    MQ135.serialDebug();
}

/*****************************MAIN*********************************************/

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
}

void loop() {
    MDNS.update();

    Debug.handle();

    //readMQSensor();

    shelly.handleStateSwitch();
}
