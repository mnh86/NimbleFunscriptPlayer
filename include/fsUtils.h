#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    Serial.printf("Listing dir: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("- failed to open dir");
        return;
    }
    if (!root.isDirectory())
    {
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            Serial.print(" DIR : ");
            Serial.println(file.name());
            if (levels)
            {
                listDir(fs, file.name(), levels - 1);
            }
        }
        else
        {
            Serial.print(" FILE : ");
            Serial.print(file.name());
            Serial.print("\t SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void readFunscriptFile(fs::FS &fs, const char *path)
{
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if (!file || file.isDirectory())
    {
        Serial.println("- failed to open file for reading");
        return;
    }

    Serial.println("- read from file:");

    StaticJsonDocument<64> filter;
    filter["at"] = true;
    filter["pos"] = true;
    StaticJsonDocument<64> json;

    while (file.available())
    {
        file.find("\"actions\":[");
        do {
            DeserializationError error = deserializeJson(json, file, DeserializationOption::Filter(filter));
            if (error == DeserializationError::Ok) {
                serializeJsonPretty(json, Serial);
            } else if (error != DeserializationError::EmptyInput) {
                Serial.println(error.f_str());
                return;
            }
        } while (file.findUntil(",", "]"));
    }

    Serial.println("\n- file read done.");
}
