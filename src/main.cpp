#include <Arduino.h>
#include <SPIFFS.h>
#include <BfButton.h>
#include <millisDelay.h>
#include "NimbleFunscript.h"
#include "fsUtils.h"

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

const char* filenames[] = {
    "/1-slowbj-7m.funscript",
    "/7-crazy-10m.funscript",
    "/2-strokes-20m.funscript",
    "/3-smooth-20m.funscript",
    "/4-intense-20m.funscript",
    "/5-invert-20m.funscript",
    "/6-topbot-20m.funscript"
};
short numFilenames = sizeof(filenames) / sizeof(const char*);
short fileIndex = 0;

const char* nextFile() {
    short i = fileIndex;
    fileIndex++;
    if (fileIndex >= numFilenames) {
        fileIndex = 0;
    }
    //Serial.printf("File index: %d, Next: %d, Size: %d\n", i, fileIndex, numFilenames);
    return filenames[i];
}

void pressHandler(BfButton *btn, BfButton::press_pattern_t pattern)
{
    switch (pattern)
    {
    case BfButton::LONG_PRESS:
        nimble.stop();
        break;

    case BfButton::DOUBLE_PRESS:
        nimble.initFunscriptFile(SPIFFS, nextFile());
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
    listDir(SPIFFS, "/", 0);
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
