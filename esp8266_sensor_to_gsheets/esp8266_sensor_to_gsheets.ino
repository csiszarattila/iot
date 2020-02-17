#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

#include <ESP8266WebServer.h>
#include <DNSServer.h>           // http://192.168.0.128//Local DNS Server used for redirecting all requests to the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include "DHTesp.h"
#include "EspHtmlTemplateProcessor.h"

ESP8266WebServer server(80);
EspHtmlTemplateProcessor templateProcessor(&server);
WiFiManager wifiManager;

#include <WiFiClientSecure.h>

WiFiClientSecure client;

// SHA1 fingerprint of the certificate, don't care with your GAS service
const char fingerprint[] PROGMEM = "3F 67 E2 30 08 19 3F 49 59 23 6B C5 E1 51 DC 43 AD DC 11 73";

/** Initialize DHT sensor */
DHTesp dht;
/** Pin number for DHT11 data pin */
int dhtPin = 4;

struct SensorData {
  float temperature;
  float humidity;
  int light;

  char temperatureInStr[8];
  char humidityInStr[8];
  char lightInStr[8];
};

struct SensorData readSensors()
{
  TempAndHumidity dhtSensorValues = dht.getTempAndHumidity();
  SensorData data;

  data.temperature = dhtSensorValues.temperature;
  data.humidity = dhtSensorValues.humidity;
  data.light = analogRead(A0);

  return data;
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);
  Serial.println("All good");

  // WIFIManager Setup
  wifiManager.autoConnect();
  SPIFFS.begin();

  dht.setup(dhtPin, DHTesp::DHT11);

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  server.onNotFound(handleNotFound);

  server.begin();
}

void sendDataToGoogleSheets()
{
  SensorData data = readSensors();

  Serial.println("connecting to script.google.com");

  client.setFingerprint(fingerprint);
  if (!client.connect("script.google.com", 443)) {
    Serial.println("connection failed to script.google.com");
    return;
  }

  //String string_distance =  String(targetDistance, DEC);
  String parameters = "";
  parameters += "?temperature=";
  parameters += data.temperature;
  parameters += "&humidity=";
  parameters += data.humidity;
  parameters += "&light=";
  parameters += data.light;

  client.println("GET /macros/s/AKfycbzEmcPgh42PULrSr2c7zzxInxNTSrg6NYtBBEWT6J1GwrAbFlE4/exec" + parameters + " HTTP/1.0");
  client.println("Host: script.google.com");
  client.println("Connection: close");
  client.println();

  Serial.println("request sent");
}

void loop() {
  server.handleClient();

  sendDataToGoogleSheets();

  delay(60*1000);
}
