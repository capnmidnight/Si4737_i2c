#include "HAL.h"
#include <I2C/I2C.h>

bool interruptReceived = true, CTS = true, needCTS = false, STC = true, needSTC = false, ERR = false;
size_t expResp = 0;

byte RESP[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };


uint16_t translateWB(uint16_t wb)
{
    return 64960 + (wb - 2400) * 2 / 5;
}

void sleep(uint16_t millis)
{
    delay(millis >> CLOCK_SHIFT);
}

void wait(uint16_t seconds)
{
    for (uint16_t i = 0; i < seconds; ++i)
    {
        sleep(1000);
    }
}

void GPO2InteruptHandler()
{
    interruptReceived = true;
}

void waitForInterrupt()
{
    while (!interruptReceived)
    {
        sleep(100);
    }
    interruptReceived = false;
}

////////////////////////////////////////////////////////////
// CHIP COMS
////////////////////////////////////////////////////////////

void transmitCommand(const char* name, byte cmd, size_t responseLength, bool ctsExpected, bool stcExpected, int arg0 = -1, int arg1 = -1, int arg2 = -1, int arg3 = -1, int arg4 = -1, int arg5 = -1, int arg6 = -1)
{
    CTS = false;
    STC = false;
    ERR = false;
    needCTS = ctsExpected;
    needSTC = stcExpected;
    expResp = responseLength;
    static int args[MAX_NUM_ARGS];
    static byte params[MAX_NUM_ARGS];
    args[0] = arg0;
    args[1] = arg1;
    args[2] = arg2;
    args[3] = arg3;
    args[4] = arg4;
    args[5] = arg5;
    args[6] = arg6;

    size_t numArgs = 0;
    while (numArgs < MAX_NUM_ARGS && args[numArgs] >= 0)
    {
        params[numArgs] = args[numArgs];
        ++numArgs;
    }

    I2c.write(SI4737_BUS_ADDRESS, cmd, params, numArgs);
}

void getStatus()
{
    bool needStatus = needCTS || needSTC;
    if (needStatus)
    {
        ++expResp;
    }
    if (expResp > 0)
    {
        I2c.read(SI4737_BUS_ADDRESS, expResp, RESP);
        if (needStatus)
        {
            byte status = RESP[0];

            if (status & 0x80 && needCTS)
            {
                CTS = true;
                needCTS = false;
            }

            if (status & 0x40)
            {
                ERR = true;
            }

            if (status & 0x01 && needSTC)
            {
                STC = true;
                needSTC = false;
            }
        }

        expResp = 0;
    }
}

////////////////////////////////////////////////////////////
// CHIP API COMMANDS
////////////////////////////////////////////////////////////

void cmdPowerUp(byte funcMode, bool analogOutput, bool digitalOutput, bool enableCTSInterrupt, bool enableGP02, bool enableXOSC, bool enablePatch)
{
#define POWER_UP 0x01
#define POWER_UP_CTS_INT_EN 0x80
#define POWER_UP_GPO2_EN 0x40
#define POWER_UP_PATCH  0x20
#define POWER_UP_X_OSC_EN 0x10
#define POWER_UP_OPMODE_RDS_ONLY 0x00
#define POWER_UP_OPMODE_ANALOG 0x05
#define POWER_UP_OPMODE_DIGITAL_ON_ANALOG_PINS 0x0B
#define POWER_UP_OPMODE_DIGITAL 0xB0

    byte options = 0, outputMode = 0;

    if (enableCTSInterrupt) options |= POWER_UP_CTS_INT_EN;
    if (enableGP02) options |= POWER_UP_GPO2_EN;
    if (enablePatch) options |= POWER_UP_PATCH;
    if (enableXOSC) options |= POWER_UP_X_OSC_EN;

    if (analogOutput) outputMode |= POWER_UP_OPMODE_ANALOG;
    if (digitalOutput) outputMode |= POWER_UP_OPMODE_DIGITAL;

    transmitCommand("POWER_UP", POWER_UP, 0, true, false, options | funcMode, outputMode);
}

void powerUp(byte funcMode, bool analogOutput, bool digitalOutput, bool enableCTSInterrupt, bool enableGP02, bool enableXOSC, bool enablePatch)
{
    cmdPowerUp(funcMode, analogOutput, digitalOutput, enableCTSInterrupt, enableGP02, enableXOSC, enablePatch);
    waitForInterrupt();
    getStatus();
}

void powerDown()
{
#define POWER_DOWN 0x11

    transmitCommand("POWER_DOWN", POWER_DOWN, 0, false, false);
}

void setProperty(const char* name, uint16_t propertyNumber, uint16_t propertyValue)
{
#define SET_PROPERTY 0x12

    transmitCommand(
        name,
        SET_PROPERTY, 0, false, false,
        0x00, // always 0
        highByte(propertyNumber), lowByte(propertyNumber),
        highByte(propertyValue), lowByte(propertyValue));
    // always delay 10ms after SET_PROPERTY. It has no CTS response,
    // but the 10ms is guaranteed.
    delay(10);
}

void cmdGetProperty(const char* name, uint16_t propertyNumber)
{
#define GET_PROPERTY 0x13

    transmitCommand(name,
        GET_PROPERTY, 3, true, false,
        0x00, // always 0
        highByte(propertyNumber), lowByte(propertyNumber));
}

uint16_t getProperty(const char* name, uint16_t propertyNumber)
{
    cmdGetProperty(name, propertyNumber);
    waitForInterrupt();
    getStatus();
    uint16_t value = makeWord(RESP[2], RESP[3]);
    return value;
}

void cmdGetInterruptStatus()
{
#define GET_INT_STATUS 0x14

    transmitCommand(
        "GET_INT_STATUS",
        GET_INT_STATUS, 0, true, true);
}

bool getInterruptStatus()
{
    cmdGetInterruptStatus();
    waitForInterrupt();
    getStatus();
    return STC;
}

void waitForSTC()
{
    waitForInterrupt();
    getStatus();
    while (!ERR && !getInterruptStatus())
    {
        sleep(250);
    }
}

void cmdSetFMTuneFrequency(uint16_t f, bool freezeMetrics, bool fastTune, byte antennaCapValue)
{
#define FM_TUNE_FREQ 0x20
#define FM_TUNE_FREQ_FREEZE 0x02
#define FM_TUNE_FREQ_FAST 0x01

    byte options = 0;
    if (freezeMetrics) options |= FM_TUNE_FREQ_FREEZE;
    if (fastTune) options |= FM_TUNE_FREQ_FAST;

    transmitCommand(
        "FM_TUNE_FREQ",
        FM_TUNE_FREQ, 0, true, false,
        options,
        highByte(f),
        lowByte(f),
        antennaCapValue);
}

void setFMTuneFrequency(uint16_t f, bool freezeMetrics, bool fastTune, byte antennaCapValue)
{
    cmdSetFMTuneFrequency(f, freezeMetrics, fastTune, antennaCapValue);
    sleep(100);
    waitForSTC();
}

void cmdSetWBTuneFrequency(uint16_t f)
{
#define WB_TUNE_FREQ 0x50

    f = translateWB(f);

    transmitCommand(
        "WB_TUNE_FREQ",
        WB_TUNE_FREQ, 0, true, false,
        0x00, // always write 0
        highByte(f),
        lowByte(f));
}

void setWBTuneFrequency(uint16_t f)
{
    cmdSetWBTuneFrequency(f);
    waitForSTC();
}

void cmdSeekFM(bool seekUp, bool wrap)
{
#define FM_SEEK_START 0x21
#define FM_SEEK_START_UP 0x08
#define FM_SEEK_START_WRAP 0x04

    byte options = 0;
    if (seekUp) options |= FM_SEEK_START_UP;
    if (wrap)options |= FM_SEEK_START_WRAP;

    transmitCommand(
        "FM_SEEK_START",
        FM_SEEK_START, 0, true, false,
        options);
}

void seekFM(bool seekUp, bool wrap)
{
    cmdSeekFM(seekUp, wrap);
    waitForSTC();
}

void cmdGetFMTuneStatus(bool cancelSeek, bool ackSTC)
{
#define FM_TUNE_STATUS 0x22
#define FM_TUNE_STATUS_CANCEL 0x02
#define FM_TUNE_STATUS_INTACK 0x01

    byte options = 0;
    if (cancelSeek) options |= FM_TUNE_STATUS_CANCEL;
    if (ackSTC) options |= FM_TUNE_STATUS_INTACK;

    transmitCommand(
        "FM_TUNE_STATUS",
        FM_TUNE_STATUS, 7, true, false,
        options);
}

byte getFMTuneStatus(bool cancelSeek, bool ackSTC)
{
    cmdGetFMTuneStatus(cancelSeek, ackSTC);
    waitForInterrupt();
    getStatus();
    return RESP[4];
}

void cmdGetWBTuneStatus(bool ackSTC)
{
#define WB_TUNE_STATUS 0x52
#define WB_TUNE_STATUS_INTACK 0x01

    byte options = 0;
    if (ackSTC) options |= FM_TUNE_STATUS_INTACK;

    transmitCommand(
        "WB_TUNE_STATUS",
        WB_TUNE_STATUS, 5, true, false,
        options);
}

byte getWBTuneStatus(bool ackSTC)
{
    cmdGetWBTuneStatus(ackSTC);
    waitForInterrupt();
    getStatus();
    return RESP[4];
}

void cmdSetGPIOModes(bool enableGPIO1, bool enableGPIO2, bool enableGPIO3)
{
#define GPIO_CTL 0x80
#define GPIO_CTL_GPO3OEN 0x08
#define GPIO_CTL_GPO2OEN 0x04
#define GPIO_CTL_GPO1OEN 0x02

    byte options = 0;
    if (enableGPIO1) options |= GPIO_CTL_GPO1OEN;
    if (enableGPIO2) options |= GPIO_CTL_GPO2OEN;
    if (enableGPIO3) options |= GPIO_CTL_GPO3OEN;

    transmitCommand(
        "GPIO_CTL",
        GPIO_CTL, 0, true, false,
        options);
}

void setGPIOModes(bool enableGPIO1, bool enableGPIO2, bool enableGPIO3)
{
    cmdSetGPIOModes(enableGPIO1, enableGPIO2, enableGPIO3);
    waitForInterrupt();
}

void cmdSetGPIOLevel(bool highGPIO1, bool highGPIO2, bool highGPIO3)
{
#define GPIO_SET 0x81
#define GPIO_SET_GPO3LEVEL 0x08
#define GPIO_SET_GPO2LEVEL 0x04
#define GPIO_SET_GPO1LEVEL 0x02

    byte options = 0;
    if (highGPIO1) options |= GPIO_SET_GPO1LEVEL;
    if (highGPIO2) options |= GPIO_SET_GPO2LEVEL;
    if (highGPIO3) options |= GPIO_SET_GPO3LEVEL;

    transmitCommand(
        "GPIO_SET",
        GPIO_SET, 0, true, false,
        options);
}

void setGPIOLevel(bool highGPIO1, bool highGPIO2, bool highGPIO3)
{
    cmdSetGPIOLevel(highGPIO1, highGPIO2, highGPIO3);
    waitForInterrupt();
}

////////////////////////////////////////////////////////////
// CHIP API PROPERTIES
////////////////////////////////////////////////////////////

void setGPIOInterruptSources(bool repeatRSQ, bool repeatRDS, bool repeatSTC, bool enableCTS, bool enableERR, bool enableRSQ, bool enableRDS, bool enableSTC)
{
#define GPIO_IEN 0x0001
#define GPIO_IEN_RSQREP 0x0800
#define GPIO_IEN_RDSREP 0x0400
#define GPIO_IEN_STCREP 0x0100
#define GPIO_IEN_CTSIEN 0x0080
#define GPIO_IEN_ERRIEN 0x0040
#define GPIO_IEN_RSQIEN 0x0008
#define GPIO_IEN_RDSIEN 0x0004
#define GPIO_IEN_STCIEN 0x0001

    uint16_t value = 0;
    if (repeatRSQ) value |= GPIO_IEN_RSQREP;
    if (repeatRDS) value |= GPIO_IEN_RSQREP;
    if (repeatSTC) value |= GPIO_IEN_STCREP;
    if (enableCTS) value |= GPIO_IEN_CTSIEN;
    if (enableERR) value |= GPIO_IEN_ERRIEN;
    if (enableRSQ) value |= GPIO_IEN_RSQIEN;
    if (enableRDS) value |= GPIO_IEN_RDSIEN;
    if (enableSTC) value |= GPIO_IEN_STCIEN;

    setProperty("GPIO_IEN", GPIO_IEN, value);
}

void prepareChip()
{
    pinMode(SI4737_PIN_RESET, OUTPUT);
    digitalWrite(SI4737_PIN_RESET, LOW);
    sleep(500);
    digitalWrite(SI4737_PIN_RESET, HIGH);
    sleep(1);
    attachInterrupt(digitalPinToInterrupt(GPO2_INTERUPT_PIN), GPO2InteruptHandler, FALLING);
    I2c.begin();
}