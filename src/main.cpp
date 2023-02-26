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
    nimble.updateNetworkLEDs(0, 0);
}

const char* filenames[] = {
    "/1-slowbj-7m.funscript",
    "/2-strokes-20m.funscript",
    "/3-smooth-20m.funscript",
    "/4-intense-20m.funscript",
    "/5-invert-20m.funscript",
    "/6-topbot-20m.funscript"
};

short fileIndex = 0;
const char* nextFile() {
    short i = fileIndex;
    fileIndex++;
    if (fileIndex >= sizeof(filenames)) {
        fileIndex = 0;
    }
    return filenames[i];
}

void pressHandler(BfButton *btn, BfButton::press_pattern_t pattern)
{
    switch (pattern)
    {
    case BfButton::LONG_PRESS:
        nimble.stop();
        break;

    case BfButton::SINGLE_PRESS:
        nimble.stop();
        nimble.initFunscriptFile(SPIFFS, nextFile());
        nimble.start();
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

    btn.onPress(pressHandler).onPressFor(pressHandler, 2000);
    ledUpdateDelay.start(30);
}

void loop()
{
    btn.read();
    nimble.updateActuator();
    updateLEDs();
}
