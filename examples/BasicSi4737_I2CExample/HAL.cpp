// 
// 
// 

#include "HAL.h"
#include <Wire.h>

bool interruptReceived = true, CTS = true, needCTS = false, STC = true, needSTC = false, ERR = false;
int expResp = 0;
uint8_t clockPinState = LOW;

byte RESP[15] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };


uint16_t translateWB(uint16_t wb)
{
    return 64960 + (wb - 162400) / 25;
}

void sleep(uint16_t millis)
{
    delay(millis >> CLOCK_SHIFT);
}

void wait(uint16_t seconds)
{
    for (uint16_t i = 0; i < seconds; ++i)
    {
        Serial.print('.');
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

void printBuffer(const char* name, const size_t size, const byte* buffer, int radix = 16)
{
    Serial.print(name);
    Serial.print('(');
    if (size > 0)
    {
        for (size_t i = 0; i < size; ++i)
        {
            if (radix == 16)
            {
                Serial.print("0x");
                if (buffer[i] < radix)
                {
                    Serial.print(0);
                }
            }
            Serial.print(buffer[i], radix);
            if (radix == 2)
            {
                Serial.print('b');
            }
            if (i < (size - 1))
            {
                Serial.print(", ");
            }
        }
    }
    Serial.print(')');
}

////////////////////////////////////////////////////////////
// CHIP COMS
////////////////////////////////////////////////////////////

void transmitCommand(const char* name, byte cmd, int responseLength, bool ctsExpected, bool stcExpected, int arg0 = -1, int arg1 = -1, int arg2 = -1, int arg3 = -1, int arg4 = -1, int arg5 = -1, int arg6 = -1)
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

    Serial.print("CMD ");
    printBuffer(name, numArgs, params);
    Wire.beginTransmission(SI4737_BUS_ADDRESS);
    Wire.write(cmd);
    if (numArgs > 0)
    {
        Wire.write(params, numArgs);
    }

    byte status = Wire.endTransmission(false);

    Serial.print("I2C status: ");
    Serial.print(status);
    Serial.print(" -> ");
    switch (status)
    {
    case 0: Serial.print("OK"); break;
    case 1: Serial.print("ERR: data too long to fit in transmit buffer."); break;
    case 2: Serial.print("ERR: received NACK on transmit of address."); break;
    case 3: Serial.print("ERR: received NACK on transmit of data."); break;
    default: Serial.print("ERR: unknown."); break;
    }
    Serial.println();
}

void getStatus()
{
    bool needStatus = needCTS || needSTC;
    if (expResp > 0 || needStatus)
    {
        Serial.print("... ");
        Wire.requestFrom(SI4737_BUS_ADDRESS, expResp + (needStatus ? 1 : 0));
        while (Wire.available() < expResp)
        {
            Serial.print('.');
            sleep(100);
        }

        if (needStatus)
        {
            byte status = Wire.read();

            if (status & 0x80)
            {
                Serial.print("CTS ");
                if (status & 0x80)
                {
                    CTS = true;
                    needCTS = false;
                }
            }

            if (status & 0x40)
            {
                Serial.print("ERROR ");
                ERR = true;
            }

            if (status & 0x01)
            {
                Serial.print("STC ");
                if (needSTC)
                {
                    STC = true;
                    needSTC = false;
                }
            }
        }

        if (expResp > 0)
        {
            Wire.readBytes(RESP, expResp);
            printBuffer("RESP", expResp, RESP);
        }
        expResp = 0;
        Serial.println();
    }
}

void printStation()
{
    Serial.print("Station: ");
    Serial.println(makeWord(RESP[1], RESP[2]));
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

void cmdGetRevision(bool isSI4705or06)
{
#define GET_REV 0x10

    transmitCommand("GET_REV", GET_REV, isSI4705or06 ? 15 : 8, true, false);
}

void printRevision(bool isSI4705or06)
{
    cmdGetRevision(isSI4705or06);
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

    Serial.print("SET_PROPERTY: ");
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

    Serial.print("GET_PROPERTY: ");
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
    uint16_t value = makeWord(RESP[1], RESP[2]);
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
        sleep(1000);
    }
}

void cmdSetFMTuneFrequency(int f, bool freezeMetrics, bool fastTune, byte antennaCapValue)
{
#define FM_TUNE_FREQ 0x20
#define FM_TUNE_FREQ_FREEZE 0x02
#define FM_TUNE_FREQ_FAST 0x01

    byte options = 0;
    if (freezeMetrics) options |= FM_TUNE_FREQ_FREEZE;
    if (fastTune) options |= FM_TUNE_FREQ_FAST;

    Serial.print(f);
    Serial.print(' ');

    transmitCommand(
        "FM_TUNE_FREQ",
        FM_TUNE_FREQ, 0, true, false,
        options,
        highByte(f),
        lowByte(f),
        antennaCapValue);
}

void setFMTuneFrequency(int f, bool freezeMetrics, bool fastTune, byte antennaCapValue)
{
    cmdSetFMTuneFrequency(f, freezeMetrics, fastTune, antennaCapValue);
    waitForSTC();
}

void cmdSetWBTuneFrequency(int f)
{
#define WB_TUNE_FREQ 0x50

    Serial.print(f);
    Serial.print(" -> ");
    f = translateWB(f);
    Serial.print(f);
    Serial.print(" ");

    transmitCommand(
        "WB_TUNE_FREQ",
        WB_TUNE_FREQ, 0, true, false,
        0x00, // always write 0
        highByte(f),
        lowByte(f));
}

void setWBTuneFrequency(int f)
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
    printStation();
    return RESP[3];
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
    printStation();
    return RESP[3];
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

void setReferenceClockFrequency(uint16_t value)
{
#define REFCLK_FREQ 0x0201

    setProperty("REFCLK_FREQ", REFCLK_FREQ, value);
}

void setReferenceClockPrescale(uint16_t value, bool useDCLK)
{
#define REFCLK_PRESCALE 0x0202
#define REFCLK_PRESCALE_RCLKSEL_RCLK 0x0000
#define REFCLK_PRESCALE_RCLKSEL_DCLK 0x1000

    if (useDCLK) value |= REFCLK_PRESCALE_RCLKSEL_DCLK;

    setProperty("REFCLK_PRESCALE", REFCLK_FREQ, value);
}

void setVolume(byte value)
{
#define RX_VOLUME 0x4000

    setProperty("RX_VOLUME", RX_VOLUME, value);
}

void setHardMute(bool muteLeft, bool muteRight)
{
#define RX_HARD_MUTE 0x4001
#define RX_HARD_MUTE_LEFT 0x0002
#define RX_HARD_MUTE_RIGHT 0x0001

    byte value = 0;
    if (muteLeft) value |= RX_HARD_MUTE_LEFT;
    if (muteRight) value |= RX_HARD_MUTE_RIGHT;

    setProperty("RX_HARD_MUTE", RX_HARD_MUTE, value);
}

void prepareChip()
{
    Serial.println("Si4737 setup sequence:");

    Serial.print("Set RSTb LOW, set power HIGH... ");
    pinMode(SI4737_PIN_RESET, OUTPUT);
    digitalWrite(SI4737_PIN_RESET, LOW);
    Serial.println("OK");

    Serial.print("Wait 250ms for VDD and VIO to stablize... ");
    sleep(250);
    Serial.println("OK");

    Serial.print("Use default bus mode and wait 100ms to stabalize... ");
    sleep(100);
    Serial.println("OK");

#ifdef USE_SOFTWARE_CLOCK
    Serial.print("Start the clock signal and wait 500ms for it to stabalize... ");
    Serial.flush();
    pinMode(CLOCK_PIN, OUTPUT);
    cli(); //stop interrupts
    OCR1A = 4 >> CLOCK_SHIFT;
    TCCR1A = (1 << WGM11);
    TCCR1B = (1 << CS10);
    TIMSK1 = (1 << OCIE1A);
    sei(); //allow interrupts
    sleep(500);
    Serial.println("OK");
#endif

    Serial.print("Set RESTb HIGH and wait at least 10ns... ");
    digitalWrite(SI4737_PIN_RESET, HIGH);
    sleep(1);
    Serial.println("OK");
    Serial.println("Si4737 setup sequence complete!");
    Serial.println();

    attachInterrupt(digitalPinToInterrupt(GPO2_INTERUPT_PIN), GPO2InteruptHandler, FALLING);

    Wire.begin();
}

#ifdef USE_SOFTWARE_CLOCK
ISR(TIMER1_COMPA_vect)
{
    digitalWrite(CLOCK_PIN, clockPinState = !clockPinState);
}
#endif