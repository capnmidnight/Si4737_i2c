/*
Name:		BasicSi4737_I2CExample.ino
Created:	9/28/2015 3:00:31 PM
Author:	Sean
*/

#include "HAL.h"
#include <Wire.h>

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

    prepareChip();

    powerUp(POWER_UP_FUNC_FM_RCV);
    setGPIOModes(true, true, true);
    setGPIOInterruptSources(false, false, false, true, true, true, false, true);
    setReferenceClockPrescale(1);
    setReferenceClockFrequency(31250);
    setFMTuneFrequency(8850);
}

void loop()
{
    Serial.println(getFMTuneStatus());
    wait(5);
    seekFM(true, true);
}