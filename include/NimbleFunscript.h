#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <CircularBuffer.h>
#include "nimbleConModule.h"

#define MAX_POSITION_DELTA 50
#define VIBRATION_MAX_AMP 25
#define VIBRATION_MAX_SPEED 20.0 // hz

struct nimbleFrameState {
    int16_t targetPos = 0; // target position from tcode commands
    int16_t position = 0; // next position to send to actuator (-1000 to 1000)
    int16_t lastPos = 0; // previous frame's position
    int16_t force = IDLE_FORCE; // next force value to send to actuator (0 to 1023)
    int8_t air = 0; // next air state to send to actuator (-1 = air out, 0 = stop, 1 = air in)
    int16_t vibrationPos = 0; // next vibration position
};

class Keyframe {
    public:
        Keyframe(int at = 0, short pos = 50) { set(at, pos); }
        ~Keyframe() {};

        void set(int at, short pos) {
            _at = at;
            _pos = pos;
        }
        void copy(Keyframe k) {
            set(k.at(), k.pos());
        }
        short lerpToPos(int t, Keyframe k) {
            return map(t, at(), k.at(), pos(), k.pos());
        }
        bool equals(Keyframe k) {
            return (at() == k.at() || pos() == k.pos());
        }

        int at() { return _at; }
        short pos() { return _pos; }

    private:
        short _pos = 50;
        int _at = 0;
};

class NimbleFunscript {
    public:
        NimbleFunscript() {
            jsonFilter["at"] = true;
            jsonFilter["pos"] = true;
        }
        ~NimbleFunscript() { reset(); }
        void init();
        void start();
        void stop();
        void toggle() { if (isRunning()) stop(); else start(); }
        bool isRunning() { return running; }
        void initFunscriptFile(fs::FS &fs, const char *path);
        void updateActuator();
        void updateEncoderLEDs(bool isOn = true);
        void updateHardwareLEDs();
        void updateNetworkLEDs(uint32_t bluetooth = 0, uint32_t wifi = 0);
        void setVibrationSpeed(float v) { vibrationSpeed = min(max(v, (float)0), (float)VIBRATION_MAX_SPEED); }
        void setVibrationAmplitude(uint16_t v) { vibrationAmplitude = min(max(v, (uint16_t)0), (uint16_t)VIBRATION_MAX_AMP); }
        void printFrameState(Print& out = Serial);

    private:
        static const int START_OFFSET = 1000; // 1 sec to allow transition at start

        File currentFile;
        bool running = false;
        bool started = false;
        bool endOfActions = false;
        float vibrationSpeed = VIBRATION_MAX_SPEED; // hz
        uint16_t vibrationAmplitude = 0; // amplitude in position units (0 to 25)
        nimbleFrameState frame;
        Keyframe currentKeyframe;
        Keyframe nextKeyframe;
        CircularBuffer<Keyframe*, 32> keyBuffer;
        long startTime;
        long stopTime;

        StaticJsonDocument<64> jsonFilter;
        StaticJsonDocument<64> actionJson;

        void reset();
        int16_t clampPositionDelta();
        void processFunscriptFile();
        void lerpKeyframes();
        void handlePositionChanges();
};

void NimbleFunscript::init()
{
    initNimbleConModule();
}

void NimbleFunscript::reset()
{
    currentFile.close();
    keyBuffer.clear();
    running = false;
    started = true;
    endOfActions = false;
    vibrationAmplitude = 0;
    stopTime = 0;
    frame.force = MAX_FORCE;

    // Always restart and transition from current position
    short tmpCurPos = map(frame.position, -ACTUATOR_MAX_POS, ACTUATOR_MAX_POS, 0, 100);
    currentKeyframe.set(0, tmpCurPos);
    nextKeyframe.set(0, tmpCurPos);
}

void NimbleFunscript::start()
{
    running = true;

    if (stopTime > 0) {
        if (!started) {
            // readjust start time to account for stopped time
            long pauseTime = millis() - stopTime;
            startTime += pauseTime;
        }
        stopTime = 0;
    }
}

void NimbleFunscript::stop()
{
    running = false;
    stopTime = millis();
}

void NimbleFunscript::initFunscriptFile(fs::FS &fs, const char *path)
{
    reset();
    Serial.printf("Playing file: %s\n", path);
    currentFile = fs.open(path);
    if (!currentFile || currentFile.isDirectory())
    {
        Serial.println("- failed to open file for reading");
        return;
    }
    if (!currentFile.available() || !currentFile.find("\"actions\":[")) {
        Serial.println("- failed to find Funscript actions");
        return;
    }
}

/**
 * Fill the buffer with the next actions in the file.
 * Called during loop.
 */
void NimbleFunscript::processFunscriptFile()
{
    if (!running) return;
    if (keyBuffer.isFull()) return;
    if (endOfActions || !currentFile.available()) return;

    do {
        DeserializationError error = deserializeJson(actionJson, currentFile, DeserializationOption::Filter(jsonFilter));
        if (error == DeserializationError::Ok) {
            //serializeJsonPretty(actionJson, Serial);
            keyBuffer.push(new Keyframe(
                actionJson["at"].as<int>() + START_OFFSET,
                actionJson["pos"].as<short>()
            ));
        } else if (error != DeserializationError::EmptyInput) {
            Serial.println(error.f_str());
            return;
        }
        endOfActions = !currentFile.findUntil(",", "]");
    } while (!endOfActions && !keyBuffer.isFull());

    // Don't start playing until after buffer initially filled
    if (started) {
        started = false;
        startTime = millis();
    }
}

void NimbleFunscript::lerpKeyframes()
{
    if (!running) return;
    if (started) return; // Don't start playing until initially loaded

    long now = millis() - startTime;

    // Shift keyframes and pull next action off buffer when time exceeded
    if (now >= nextKeyframe.at() && !keyBuffer.isEmpty()) {
        currentKeyframe.copy(nextKeyframe);
        Keyframe* kf = keyBuffer.shift();
        nextKeyframe.set(kf->at(), kf->pos());
        delete kf;
        // Serial.printf("KF %08d:%03d -> %08d:%03d\n",
        //     currentKeyframe.at(), currentKeyframe.pos(),
        //     nextKeyframe.at(), nextKeyframe.pos()
        // );
    }

    // Skip duplicate keyframes
    if (currentKeyframe.equals(nextKeyframe)) return;

    // Skip if at end
    if (now > nextKeyframe.at()) return;

    // Interpolate position betweeen keyframes for the current time
    short lerp = currentKeyframe.lerpToPos(now, nextKeyframe);
    frame.targetPos = map(lerp, 0, 100, -ACTUATOR_MAX_POS, ACTUATOR_MAX_POS);
    //Serial.printf("lerp = %d\n", lerp);
}

void NimbleFunscript::handlePositionChanges()
{
    if (!running) return;
    if (vibrationAmplitude > 0 && vibrationSpeed > 0) {
        int vibSpeedMillis = 1000 / vibrationSpeed;
        int vibModMillis = millis() % vibSpeedMillis;
        float tempPos = float(vibModMillis) / vibSpeedMillis;
        int vibWaveDeg = tempPos * 360;
        frame.vibrationPos = round(sin(radians(vibWaveDeg)) * vibrationAmplitude);
    } else {
        frame.vibrationPos = 0;
    }
    // Serial.printf("A:%5d S:%0.2f P:%5d\n",
    //     vibrationAmplitude,
    //     vibrationSpeed,
    //     vibrationPos
    // );

    int targetPosTmp = frame.targetPos;
    if (frame.targetPos - vibrationAmplitude < -ACTUATOR_MAX_POS) {
        targetPosTmp = frame.targetPos + vibrationAmplitude;
    } else if (frame.targetPos + vibrationAmplitude > ACTUATOR_MAX_POS) {
        targetPosTmp = frame.targetPos - vibrationAmplitude;
    }
    frame.position = targetPosTmp + frame.vibrationPos;
}

void NimbleFunscript::updateActuator()
{
    // Update interpolations
    processFunscriptFile();
    lerpKeyframes();
    handlePositionChanges();

    // Send packet of values to the actuator when time is ready
    if (checkTimer())
    {
        if (isRunning()) {
            frame.lastPos = clampPositionDelta();
            actuator.positionCommand = frame.lastPos;
            actuator.forceCommand = frame.force;
            actuator.airIn = (frame.air > 0);
            actuator.airOut = (frame.air < 0);
        } else {
            actuator.airIn = false;
            actuator.airOut = false;
            actuator.forceCommand = IDLE_FORCE;
        }
        sendToAct();
    }

    if (readFromAct()) // Read current state from actuator.
    { // If the function returns true, the values were updated.

        // Unclear yet if any action is required when tempLimiting is occurring.
        // A comparison is needed with the Pendant behavior.
        // if (actuator.tempLimiting) {
        //     setRunMode(RUN_MODE_OFF);
        // }

        // Serial.printf("A P:%4d F:%4d T:%s\n",
        //     actuator.positionFeedback,
        //     actuator.forceFeedback,
        //     actuator.tempLimiting ? "true" : "false"
        // );
    }
}

/**
 * Failsafe to limit position changes between frames to a maximum delta
 */
int16_t NimbleFunscript::clampPositionDelta()
{
    int16_t delta = frame.position - frame.lastPos;
    if (delta >= 0) {
        return (delta > MAX_POSITION_DELTA) ? frame.lastPos + MAX_POSITION_DELTA : frame.position;
    } else {
        return (delta < -MAX_POSITION_DELTA) ? frame.lastPos - MAX_POSITION_DELTA : frame.position;
    }
}

void NimbleFunscript::updateEncoderLEDs(bool isOn)
{
    int16_t pos = frame.lastPos;
    int16_t vibPos = frame.vibrationPos;

    byte ledScale = map(abs(pos), 0, ACTUATOR_MAX_POS, 1, LED_MAX_DUTY);
    byte ledState1 = 0;
    byte ledState2 = 0;
    byte vibScale = map(abs(vibPos), 0, VIBRATION_MAX_AMP, 1, LED_MAX_DUTY);
    byte vibState1 = 0;
    byte vibState2 = 0;

    if (isOn)
    {
        if (pos < 0) {
            ledState1 = ledScale;
        } else if (pos > 0) {
            ledState2 = ledScale;
        }

        if (vibPos < 0) {
            vibState1 = vibScale;
        } else if (vibPos > 0) {
            vibState2 = vibScale;
        }
    }

    ledcWrite(ENC_LED_N,  ledState1);
    ledcWrite(ENC_LED_SE, ledState1);
    ledcWrite(ENC_LED_SW, ledState1);

    ledcWrite(ENC_LED_NE, ledState2);
    ledcWrite(ENC_LED_NW, ledState2);
    ledcWrite(ENC_LED_S,  ledState2);

    ledcWrite(ENC_LED_W, vibState1);
    ledcWrite(ENC_LED_E, vibState2);
}

void NimbleFunscript::updateHardwareLEDs()
{
    ledcWrite(PEND_LED, (pendant.present) ? 50 : 0);
    ledcWrite(ACT_LED, (actuator.present) ? 50 : 0);
}

void NimbleFunscript::updateNetworkLEDs(uint32_t bluetooth, uint32_t wifi)
{
    ledcWrite(BT_LED, bluetooth);
    ledcWrite(WIFI_LED, wifi);
}
