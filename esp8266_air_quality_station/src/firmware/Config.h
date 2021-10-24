#ifndef CONFIG_H
#define CONFIG_H

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <RemoteDebug.h>

extern RemoteDebug Debug;

#define NUMBER_OF_CONFIG_ITEMS 10

class Config
{
    public:

        int ppm_limit = 1000;
        char shelly_ip[20];
        char mdns_hostname[50] = "LMSzenzor";
        bool auto_switch_enabled = true;
        int measuring_frequency = 1;
        int switch_back_time = 30;
        int required_switch_decisions = 3;

        void loadFromFile()
        {
            File configFile = LittleFS.open("./config.json", "r");
            if (!configFile) {
                debugE("Failed to open config file for reading");
                // return;
            }

            const size_t capacity = JSON_OBJECT_SIZE(NUMBER_OF_CONFIG_ITEMS) + 40;
            DynamicJsonDocument json(capacity);

            DeserializationError error = deserializeJson(json, configFile);
            if (error) {
                debugE("Failed to read file, using default configuration. Error: %s", error.c_str());
            }

            ppm_limit = json["ppm_limit"] | 4321;
            
            strlcpy(
                shelly_ip,
                json["shelly_ip"] | "192.168.0.1",
                sizeof(shelly_ip)
            );

            auto_switch_enabled = json["auto_switch_enabled"] | true;
            measuring_frequency = json["measuring_frequency"] | 1;
            switch_back_time = json["switch_back_time"] | 30;
            required_switch_decisions = json["required_switch_decisions"] | 3;

            configFile.close();
        }

        void fillFromWebsocketMessage(const JsonDocument& payload)
        {
            ppm_limit = payload["data"]["ppm_limit"];
            auto_switch_enabled = payload["data"]["auto_switch_enabled"];
            measuring_frequency = payload["data"]["measuring_frequency"];
            switch_back_time = payload["data"]["switch_back_time"];
            required_switch_decisions = payload["data"]["required_switch_decisions"];
            
            strlcpy(
                shelly_ip,
                payload["data"]["shelly_ip"],
                sizeof(shelly_ip)
            );
        }

        void saveToFile()
        {
            debugV("Saving config...");

            File configFile = LittleFS.open("./config.json", "w");
            if (!configFile) {
                debugV("Failed to open config file for writing");
                return;
            }

            const size_t capacity = JSON_OBJECT_SIZE(NUMBER_OF_CONFIG_ITEMS);
            DynamicJsonDocument json(capacity);

            json["shelly_ip"] = shelly_ip;
            json["ppm_limit"] = ppm_limit;
            json["auto_switch_enabled"] = auto_switch_enabled;
            json["measuring_frequency"] = measuring_frequency;
            json["switch_back_time"] = switch_back_time;
            json["required_switch_decisions"] = required_switch_decisions;

            if (serializeJson(json, configFile) == 0) {
                debugE("Failed to write config to file.");
            }
            
            configFile.close();
        }
};

#endif