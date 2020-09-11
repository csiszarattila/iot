#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>

/*****************************CONFIG*********************************************/
#include <FS.h>
#include <ArduinoJson.h>

struct Config {
    int ppm_limit = 1000;
    const char *shelly_ip;    
    char mdns_hostname[50] = "co2";
};

Config config;
const char *configFilename = "/config.json";

void loadConfiguration()
{
    File configFile = SPIFFS.open(configFilename, "r");
    if (!configFile) {
        Serial.println("Failed to open config file for reading");
        // return;
    }

    const size_t capacity = JSON_OBJECT_SIZE(2) + 40;
    DynamicJsonDocument json(capacity);

    DeserializationError error = deserializeJson(json, configFile);
    if (error) {
        Serial.println("Failed to read file, using default configuration.");
        Serial.println(error.c_str());
    }

    // Copy values from the JsonDocument to the Config
    config.ppm_limit = json["ppm_limit"] | 1000;
    config.shelly_ip = json["shelly_ip"] | "";

    configFile.close();
}

void saveConfiguration()
{
    Serial.println("Saving config.");

    File configFile = SPIFFS.open(configFilename, "w");
    if (!configFile) {
        Serial.println("Failed to open config file for writing");
        return;
    }

    const size_t capacity = JSON_OBJECT_SIZE(2);
    DynamicJsonDocument json(capacity);

    json["ppm_limit"] = config.ppm_limit;
    //json["shelly_ip"] = config.shelly_ip;

    if (serializeJson(json, configFile) == 0) {
        Serial.println("Failed to write config to file.");
    }
    
    configFile.close();
}

/*****************************WIFI***********************************************/
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <WiFiClient.h>

WiFiManager wifiManager;

void setupWifiManager()
{
    WiFi.hostname(config.mdns_hostname);
    wifiManager.autoConnect(config.mdns_hostname);
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void setupMDNS()
{
    if (!MDNS.begin(config.mdns_hostname)) {
        Serial.println("MDNS: start failed");
    } else {
        Serial.println("MDNS: start success");
        MDNS.addService("http", "tcp", 80);
    }
}

/*****************************HTTP SERVER****************************************/

const char INDEX_HTML[] PROGMEM = R"=="==(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/4.4.1/css/bootstrap.min.css" integrity="sha384-Vkoo8x4CGsO3+Hhxv8T/Q5PaXtkKtu6ug5TOeNV6gBiFeWPGFN9MuhOf23Q9Ifjh" crossorigin="anonymous">
    <script src="https://code.jquery.com/jquery-3.4.1.slim.min.js" integrity="sha384-J6qa4849blE2+poT4WnyKhv5vZF5SrPo0iEjwBvKU7imGFAV0wwj1yYfoRSJoZ+n" crossorigin="anonymous"></script>
    <script src="https://cdn.jsdelivr.net/npm/popper.js@1.16.0/dist/umd/popper.min.js" integrity="sha384-Q6E9RHvbIyZFJoft+2mJbHaEWldlvI9IOYy5n3zV9zzTtmI3UksdQRVvoxMfooAo" crossorigin="anonymous"></script>
    <script src="https://stackpath.bootstrapcdn.com/bootstrap/4.4.1/js/bootstrap.min.js" integrity="sha384-wfSDF2E50Y2D1uUdj0O3uMBJnjuUD4Ih7YwaYd1iqfktj0Uod8GCExl3Og8ifwB6" crossorigin="anonymous"></script>
    <link href="https://fonts.googleapis.com/icon?family=Material+Icons" rel="stylesheet">
    <title>CO2 Szenzor</title>
</head>
<body>
<div class="container">
    <h1><i class="material-icons md-48">settings</i></span>CO2 Szenzor</h1>
    <form action="" method="GET">
        <div class="form-group">
            <label>Shelly ip:</label>
            <input type="text" class="form-control" name="shelly_ip" value="{SHELLY_IP}">
        </div>
        <div class="form-group">
            <label>Határérték (ppm):</label>
            <input type="text" class="form-control" name="ppm_limit" value="{PPM_LIMIT}">
        </div>
        <button type="submit" class="btn btn-primary">Mentés</button>
    </form>
</div>
</body>
</html>
)=="==";

ESP8266WebServer httpServer(80);

void setupHttpServer()
{
  httpServer.onNotFound(handleNotFound);
  httpServer.on("/", handleIndexPage);
  httpServer.begin();
}

void handleNotFound() {
  httpServer.send(404, "text/plain", "Not found");
}

void handleIndexPage()
{
  String page = FPSTR(INDEX_HTML);

  if (!httpServer.arg("ppm_limit").isEmpty()) {
      config.ppm_limit = httpServer.arg("ppm_limit").toInt();
      //httpServer.arg("shelly_ip").toCharArray(config.shelly_ip, sizeof(config.shelly_ip));

      saveConfiguration();
  }

  page.replace("{PPM_LIMIT}", String(config.ppm_limit));
  page.replace("{SHELLY_IP}", config.shelly_ip);

  httpServer.send(200, "text/html", page);
}

/*****************************Shelly Switch**************************************/

void switchOn()
{
    WiFiClient wifiClient;
    HTTPClient httpClient;

    httpClient.begin(wifiClient, "http://192.168.0.138/relay/0?turn=on");
    int responseCode = httpClient.GET();

    if (responseCode < 0) {
        Serial.print("Switch: Error code");
        Serial.println(responseCode);
    }

    httpClient.end();
}

void switchOff()
{
    WiFiClient wifiClient;
    HTTPClient httpClient;

    httpClient.begin(wifiClient, "http://192.168.0.138/relay/0?turn=off");
    int responseCode = httpClient.GET();

    if (responseCode < 0) {
        Serial.print("Switch: Error code");
        Serial.println(responseCode);
    }

    httpClient.end();
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
    loadConfiguration();

    setupWifiManager();
    setupHttpServer();
    setupMDNS();
    //setupMQSensor();
}

void loop() {
    httpServer.handleClient();
    MDNS.update();

    //readMQSensor();
}
