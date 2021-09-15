#ifndef SENSOR_HISTORY_H
#define SENSOR_HISTORY_H

#include "LinkedList.h"
#include <HardwareSerial.h>

enum SwitchAIDecision {
    UNKNOWN = '-',
    SWITCH_OFF = 'K',
    SWITCH_ON = 'B',
    PROGRESSIVE_MEASURE = 'P',
    WAITING = 'V'
};

typedef struct Sensors {
    unsigned long at = 0;
    float pm1 = 0.0;
    float pm25 = 0.0;
    float pm4 = 0.0;
    float pm10 = 0.0;
    float temp = 0.0;
    float humidity = 0.0;
    SwitchAIDecision switch_ai_decision = UNKNOWN;

    int aqi() {
        return (int)(pm1 + pm25 + pm4 + pm10) / 4;
    }
} Sensors;


#define SENSOR_HISTORY_SIZE 100
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
            response->print("Idopont;Pm1;Pm2.5;Pm4;Pm10;AQI;Ai\n");

            File historyFile = LittleFS.open("/sensors.txt", "r");
            while (historyFile.available()) {
                Sensors data;
                historyFile.read((byte *)&data, sizeof(data));

                char formattedAt[64];
                time_t at = data.at;
                struct tm* tm = localtime(&at);
                strftime(formattedAt, sizeof(formattedAt), "%Y-%m-%dT%H:%M:%S+00:00", tm);

                response->printf(
                    "\"%s\";%.2f;%.2f;%.2f;%.2f;%d;%s;\n",
                    formattedAt,
                    data.pm1,
                    data.pm25,
                    data.pm4,
                    data.pm10,
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
};

#endif