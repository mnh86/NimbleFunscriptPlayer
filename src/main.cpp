#include <Arduino.h>
#include <SPIFFS.h>
#include <BfButton.h>
#include <millisDelay.h>
#include "NimbleFunscript.h"
#include "fsUtils.h"

NimbleFunscript nimble;

millisDelay ledUpdateDelay;

BfButton btn(BfButton::STANDALONE_DIGITAL, ENC_BUTT, true, LOW);

void pressHandler(BfButton *btn, BfButton::press_pattern_t pattern)
{
    switch (pattern)
    {
    case BfButton::SINGLE_PRESS:
        nimble.toggle();
        break;
    }
}

void updateLEDs()
{
    if (!ledUpdateDelay.justFinished()) return;
    ledUpdateDelay.repeat();

    nimble.updateEncoderLEDs();
    nimble.updateHardwareLEDs();
    nimble.updateNetworkLEDs(0, 0);
}

void setup()
{
    nimble.init();
    while (!Serial);

    if (!SPIFFS.begin(true)) {
        Serial.println("An error occurred while mounting SPIFFS");
    }
    listDir(SPIFFS, "/", 0);
    nimble.initFunscriptFile(SPIFFS, "/1-slowbj-7m.funscript");
    //nimble.initFunscriptFile(SPIFFS, "/5-invert-20m.funscript");
    nimble.start();

    btn.onPress(pressHandler);
    ledUpdateDelay.start(30);
}

void loop()
{
    btn.read();
    nimble.updateActuator();
    updateLEDs();
}
