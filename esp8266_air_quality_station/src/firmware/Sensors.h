#ifndef SENSOR_HISTORY_H
#define SENSOR_HISTORY_H

#include "LinkedList.h"
#include <HardwareSerial.h>

enum SwitchAIDecision {
    UNKNOWN = 'o',
    SWITCH_OFF = 'K',
    SWITCH_ON = 'B',
    PROGRESSIVE_MEASURE = 'P',
    WAITING = 'V'
};

typedef struct Sensors {
    unsigned long at = 0;
    float pm10 = -1.0;
    float pm25 = -1.0;
    float temp = 0.0;
    float humidity = 0.0;
    SwitchAIDecision switch_ai_decision = UNKNOWN;

    int aqi() {
        return (int)(pm10 + pm25) / 2;
    }
} Sensors;


#define SENSOR_HISTORY_SIZE 50
class SensorsHistory
{
    public:
        ALinkedList<Sensors> items = ALinkedList<Sensors>();

        void addData(Sensors data)
        {
            if (items.size() == SENSOR_HISTORY_SIZE) {
                items.remove(0);
            }
            items.add(data);
            
            store();
        }

        void store()
        {
            File historyFile = LittleFS.open("/sensors.txt", "w+");

            for (int idx = 0; idx < items.size(); idx++) {
                Sensors data = items.get(idx);

                historyFile.write((byte*)&data, sizeof(data));
            }

            historyFile.close();
        }

        void restore()
        {
            File historyFile = LittleFS.open("/sensors.txt", "r");
            
            while (historyFile.available()) {
                Sensors data;
                historyFile.read((byte *)&data, sizeof(data));

                items.add(data);
            }

            historyFile.close();
        }

        Sensors last()
        {
            return items.get(items.size() - 1);
        }

        bool isEmpty()
        {
            return items.size() <= 0;
        }

        void printToResponse(AsyncResponseStream *response)
        {
            response->print("Idopont;Pm10;Pm25;AQI;Ai\n");

            File historyFile = LittleFS.open("/sensors.txt", "r");
            while (historyFile.available()) {
                Sensors data;
                historyFile.read((byte *)&data, sizeof(data));

                char formattedAt[64];
                time_t at = data.at;
                struct tm* tm = localtime(&at);
                strftime(formattedAt, sizeof(formattedAt), "%Y-%m-%dT%H:%M:%S+00:00", tm);

                response->printf(
                    "\"%s\";%.2f;%.2f;%d;%s;\n",
                    formattedAt,
                    data.pm10,
                    data.pm25,
                    data.aqi(),
                    data.switch_ai_decision == SWITCH_OFF ? "Ki\0"
                    : data.switch_ai_decision == SWITCH_ON ? "Be\0"
                    : data.switch_ai_decision == PROGRESSIVE_MEASURE ? "10%\0"
                    : data.switch_ai_decision == WAITING ? "Varakozas\0"
                    : "-\0"
                );
            }

            historyFile.close();
        }

        void print()
        {
            Serial.println("---S---");
            Serial.println(items.size());
            Serial.println("------");

            char itemTemplate[] = R"===("at":%d, "aqi":%d, "pm10":%.2f, "pm25":%.2f, "temp":"%.2f")===";

            for (int idx = 0; idx < items.size(); idx++) {
                char buf[150];
                Sensors data = items.get(idx);
                
                snprintf(
                    buf,
                    150,
                    itemTemplate,
                    data.at,
                    data.aqi(),
                    data.pm10,
                    data.pm25,
                    data.temp
                );

                Serial.println(buf);
            }
            Serial.println("------");
        }
};

#endif