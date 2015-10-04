/*
Name:		BasicSi4737_I2CExample.ino
Created:	9/28/2015 3:00:31 PM
Author:	Sean
*/

#include <I2C.h>
#include "HAL.h"

#define DEBOUNCE_COUNT 3
#define SIGNAL_PIN 3
#define FM_MODE_PIN 12
#define WB_MODE_PIN 13

#define DIGIT_BIT_COUNT 4
#define DIGIT_BIT_PIN0 7
#define DIGIT_BIT_PIN1 6
#define DIGIT_BIT_PIN2 5
#define DIGIT_BIT_PIN3 4

#define DIGIT_SEL_COUNT 4
#define DIGIT_SEL_PIN0 8
#define DIGIT_SEL_PIN1 9
#define DIGIT_SEL_PIN2 10
#define DIGIT_SEL_PIN3 11

const int digitSelectPins[DIGIT_SEL_COUNT] = { DIGIT_SEL_PIN0, DIGIT_SEL_PIN1, DIGIT_SEL_PIN2, DIGIT_SEL_PIN3 };
const int digitBitPins[DIGIT_BIT_COUNT] = { DIGIT_BIT_PIN0, DIGIT_BIT_PIN1, DIGIT_BIT_PIN2, DIGIT_BIT_PIN3 };

uint16_t oldFrequency = 0;
byte oldMode = POWER_UP_FUNC_NONE;

uint16_t rawReadFrequency()
{
    uint16_t frequency = 0;
    for (size_t s = 0; s < DIGIT_SEL_COUNT; ++s)
    {
        digitalWrite(digitSelectPins[s], HIGH);
        for (size_t b = 0; b < DIGIT_BIT_COUNT; ++b)
        {
            uint16_t digit = (digitalRead(digitBitPins[b]) << b);
            for (size_t z = 0; z < DIGIT_SEL_COUNT - s - 1; ++z)
            {
                digit *= 10;
            }
            frequency += digit;
        }
        digitalWrite(digitSelectPins[s], LOW);
    }
    return frequency;
}

uint16_t readFrequency()
{
    uint16_t frequency = 0;
    for (int i = 0; i < DEBOUNCE_COUNT; ++i) 
    {
        frequency = max(frequency, rawReadFrequency());
        sleep(33);
    }
    return frequency;
}

void setup()
{
    // NOTE: if you're running on a 3.3v Arduino, you should double the baud rate
    // at which you run the SoftSerial library, but do not double it in the Serial
    // Monitor, because the lower voltage Arduino's only run at half the clock
    // rate but doesn't know it, so the SoftSerial code assumes it's running at full
    // speed, which screws up the timing.
    
    Serial.begin(115200 << CLOCK_SHIFT);
    Serial.println();
    Serial.println();
    Serial.println("OK");

    pinMode(FM_MODE_PIN, OUTPUT);
    pinMode(WB_MODE_PIN, OUTPUT);

    for (size_t i = 0; i < DIGIT_SEL_COUNT; ++i) {
        pinMode(digitSelectPins[i], OUTPUT);
    }
    for (size_t i = 0; i < DIGIT_BIT_COUNT; ++i) {
        pinMode(digitBitPins[i], INPUT);
    }

    prepareChip();
}

byte getModeFromFrequency(uint16_t frequency)
{
    byte mode = POWER_UP_FUNC_NONE;
    if (FM_FREQ_MIN <= frequency && frequency <= FM_FREQ_MAX)
    {
        mode = POWER_UP_FUNC_FM_RCV;
    }
    else if (WB_FREQ_MIN <= frequency && frequency <= WB_FREQ_MAX)
    {
        mode = POWER_UP_FUNC_WB_RCV;
    }
    return mode;
}

void startNewMode(byte mode)
{
    if (mode != oldMode)
    {
        if (oldMode != POWER_UP_FUNC_NONE)
        {
            Serial.println("Shutting down old mode.");
            powerDown();
            digitalWrite(oldMode == POWER_UP_FUNC_FM_RCV ? FM_MODE_PIN : WB_MODE_PIN, LOW);
        }

        if (mode != POWER_UP_FUNC_FM_RCV && mode != POWER_UP_FUNC_WB_RCV)
        {
            mode = POWER_UP_FUNC_NONE;
        }

        if (mode != POWER_UP_FUNC_NONE)
        {
            Serial.print("Starting new mode: ");
            Serial.println(mode == POWER_UP_FUNC_FM_RCV ? "FM" : "WB");
            powerUp(mode, true, false, true, true, true, false);
            setGPIOModes(true, true, true);
            sleep(500);
            digitalWrite(mode == POWER_UP_FUNC_FM_RCV ? FM_MODE_PIN : WB_MODE_PIN, HIGH);
        }
    }
}

void loop()
{
    uint16_t frequency = readFrequency();
    if (frequency != oldFrequency)
    {
        Serial.print("FREQ: ");
        Serial.println(frequency);
        byte mode = getModeFromFrequency(frequency);

        startNewMode(mode);

        if (mode == POWER_UP_FUNC_FM_RCV)
        {
            setFMTuneFrequency(frequency * 10);
        }
        else if (mode == POWER_UP_FUNC_WB_RCV)
        {
            setWBTuneFrequency(frequency);
        }

        oldMode = mode;
    }

    byte signalStrength = 0;
    if (oldMode == POWER_UP_FUNC_FM_RCV)
    {
        signalStrength = getFMTuneStatus();
    }
    else if (oldMode == POWER_UP_FUNC_WB_RCV)
    {
        signalStrength = getWBTuneStatus();
    }

    analogWrite(SIGNAL_PIN, signalStrength);
    oldFrequency = frequency;
}