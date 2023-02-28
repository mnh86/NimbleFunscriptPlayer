#include <Arduino.h>
#include <SPIFFS.h>
#include <BfButton.h>
#include <millisDelay.h>
#include "NimbleFunscript.h"

NimbleFunscript nimble;

millisDelay ledUpdateDelay;

BfButton btn(BfButton::STANDALONE_DIGITAL, ENC_BUTT, true, LOW);

void updateLEDs()
{
    if (!ledUpdateDelay.justFinished()) return;
    ledUpdateDelay.repeat();

    nimble.updateEncoderLEDs();
    nimble.updateHardwareLEDs();
    //nimble.updateNetworkLEDs();
}

const unsigned MAX_FILES = 10;
String filenames[MAX_FILES] = {};
short numFiles = 0;
short fileIndex = 0;

String nextFile() {
    short i = fileIndex;
    fileIndex++;
    if (fileIndex >= numFiles) {
        fileIndex = 0;
    }
    //Serial.printf("File index: %d, Next: %d, Size: %d\n", i, fileIndex, numFilenames);
    return filenames[i];
}

void sortFilenames()
{
    if (numFiles <= 1) return;
    for (int i = 0; i < numFiles; i++) {
        for (int j = i + 1; j < numFiles; j++) {
            if (filenames[j] < filenames[i]) {
                String q = filenames[i];
                filenames[i] = filenames[j];
                filenames[j] = q;
            }
        }
    }
}

void getFunscriptFiles(fs::FS &fs) {
    numFiles = 0;
    Serial.println("Listing dir: /");
    File root = fs.open("/");
    if (!root) {
        Serial.println("Error: failed to open dir");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("Error: not a directory");
        return;
    }
    File file = root.openNextFile();
    while (file)
    {
        if (!file.isDirectory() && numFiles < MAX_FILES) {
            String filename = "/" + String(file.name());
            if (filename.endsWith(".funscript")) {
                filenames[numFiles++] = filename;
                Serial.print(" FILE : ");
                Serial.print(file.name());
                Serial.print("\t SIZE: ");
                Serial.println(file.size());
            }
        }
        file = root.openNextFile();
    }
    sortFilenames();
}

void pressHandler(BfButton *btn, BfButton::press_pattern_t pattern)
{
    switch (pattern)
    {
    case BfButton::LONG_PRESS:
        nimble.stop();
        break;

    case BfButton::DOUBLE_PRESS:
        nimble.initFunscriptFile(SPIFFS, nextFile().c_str());
        nimble.start();
        break;

    case BfButton::SINGLE_PRESS:
        nimble.toggle();
        break;
    }
}

void setup()
{
    nimble.init();
    while (!Serial);

    if (!SPIFFS.begin(true)) {
        Serial.println("An error occurred while mounting SPIFFS");
    }
    getFunscriptFiles(SPIFFS);
    Serial.println("Ready.");

    btn.onPress(pressHandler)
        .onDoublePress(pressHandler)
        .onPressFor(pressHandler, 2000);

    ledUpdateDelay.start(30);
}

void loop()
{
    btn.read();
    nimble.updateActuator();
    updateLEDs();
}
