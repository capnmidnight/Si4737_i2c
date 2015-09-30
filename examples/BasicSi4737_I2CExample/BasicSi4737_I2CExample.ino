/*
Name:		BasicSi4737_I2CExample.ino
Created:	9/28/2015 3:00:31 PM
Author:	Sean
*/

#include <Wire.h>

#define POWER_PIN A0
#define SI4737_BUS_ADDRESS 0x11
#define SI4737_PIN_RESET A3
#define SI4737_PIN_SDIO A4
#define SI4737_PIN_SCLK A5
#define CLOCK_SHIFT 1

void prepareChip();
void wait(int);
void sleep(int);
bool translateI2CStatus(byte);
bool translateChipStatus(byte);
byte getChipStatus();
bool sendCommand(const char*, const byte, const size_t, const byte*, size_t, byte*, bool = true, bool = false);
void printResponse(const char*, const size_t, const byte*, int = 16);

void setup()
{
    Wire.begin();

    // NOTE: if you're running on a 3.3v Arduino, you should double the baud rate
    // at which you run the SoftSerial library, but do not double it in the Serial
    // Monitor, because the lower voltage Arduino's only run at half the clock
    // rate but doesn't know it, so the SoftSerial code assumes it's running at full
    // speed, which screws up the timing.
    Serial.begin(115200 << CLOCK_SHIFT);
    Serial.println();
    Serial.println();

    prepareChip();

    byte POWER_UP[] = { 0x01, 2, 1 };
    byte GET_REV[] = { 0x10, 0, 8 };

    byte ARGS[7] = { 0,0,0,0,0,0,0 };
    byte RESP[15] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
    ARGS[0] = 0x10;
    ARGS[1] = 0xB5;
    if (sendCommand("POWER_UP", 0x01, 2, ARGS, 0, RESP)
        && sendCommand("GET_REV", 0x10, 0, ARGS, 8, RESP))
    {
        printResponse("Revision", 8, RESP);

        // get volume
        ARGS[0] = 0x00; // always 0
        ARGS[1] = 0x40;
        ARGS[2] = 0x00;
        if (sendCommand("GET_PROPERTY: RX_VOLUME", 0x13, 3, ARGS, 3, RESP)) 
        {
            printResponse("Volume", 3, RESP);

            // set volume
            ARGS[0] = 0x00; // always 0
            ARGS[1] = 0x40;
            ARGS[2] = 0x00;
            ARGS[3] = 0x00;
            ARGS[4] = 0x50;
            if (sendCommand("SET_PROPERTY: RX_VOLUME", 0x12, 5, ARGS, 0, RESP, false))
            {
                if (sendCommand("GET_PROPERTY: RX_VOLUME", 0x13, 3, ARGS, 3, RESP))
                {
                    printResponse("Volume", 3, RESP);

                    ARGS[0] = 0x00; // always 0
                    ARGS[1] = (10110 &0xff00) >> 8;
                    ARGS[2] = (10110 &0x00ff);
                    ARGS[3] = 0x00; // automatically set the tuning cap
                    if (sendCommand("FM_TUNE_FREQ: 101.1", 0x20, 4, ARGS, 0, RESP))
                    {
                        if (sendCommand("GET_INT_STATUS", 0x14, 0, ARGS, 0, RESP, true, true)) {
                            Serial.print("Waiting a few seconds");
                            wait(3);
                            Serial.println(" OK");
                            ARGS[0] = 0x00;
                            if (sendCommand("GET_PROPERTY: FM_TUNE_STATUS", 0x13, 1, ARGS, 7, RESP))
                            {
                                printResponse("Tune", 7, RESP);
                                if (sendCommand("POWER_DOWN", 0x11, 0, ARGS, 0, RESP))
                                {
                                    Serial.println("Successfully shut down.");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void sleep(int millis)
{
    delay(millis >> CLOCK_SHIFT);
}

void wait(int seconds) 
{
    for (int i = 0; i < seconds; ++i) 
    {
        Serial.print('.');
        sleep(1000);
    }
}

void printResponse(const char* name, const size_t numResp, const byte* resp, int radix)
{
    Serial.print(name);
    Serial.print(": ");
    for (int i = 0; i < numResp; ++i)
    {
        Serial.print(resp[i], radix);
        Serial.print(' ');
    }
    Serial.println();
}

bool sendCommand(const char* name, const byte cmd, const size_t numArgs, const byte* args, size_t numResp, byte* resp, bool checkCTS, bool checkSTC)
{
    bool extraByte = checkCTS || checkSTC;
    if (extraByte) ++numResp;
    Serial.print("Sending ");
    Serial.print(name);
    Serial.print("... ");
    Wire.beginTransmission(SI4737_BUS_ADDRESS);
    Wire.write(cmd);
    if (numArgs > 0)
    {
        Wire.write(args, numArgs);
    }
    byte status = Wire.endTransmission(false);
    bool i2cGood = translateI2CStatus(status);
    bool ctsGood = !checkCTS;
    bool stcGood = !checkSTC;
    // try only 3 times before giving up, we don't want to spin forever
    for (int i = 0; i < 3 && i2cGood && (!ctsGood || !stcGood); ++i)
    {
        // always sleep right away, to give the chip some time to do
        // do whatever we asked it to do.
        sleep(100);
        Serial.print("Retrieving response..");
        Wire.requestFrom(SI4737_BUS_ADDRESS, numResp);
        while (Wire.available() < numResp)
        {
            Serial.print('.');
            sleep(100);
        }
        if (extraByte)
        {
            --numResp;
            byte status = Wire.read();
            Serial.print(' ');
            Serial.print(status, 2);
            Serial.print(" -> ");

            if (checkCTS && (status & 0x80) != 0) 
            {
                ctsGood = true;
            }

            if (checkSTC && (status & 0x01) != 0)
            {
                stcGood = true;
            }

            if (status & 0x80)
            {
                Serial.print("CTS ");
            }

            if (status & 0x40)
            {
                Serial.print("ERROR ");
            }

            if (status & 0x01)
            {
                Serial.print("CMD_COMPLETE ");
            }

            Serial.println();
        }
        Wire.readBytes(resp, numResp);
    }
    return i2cGood && ctsGood && stcGood;
}

bool translateChipStatus(const byte status)
{
    return status == 0x80;
}

bool translateI2CStatus(const byte status)
{
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
    return status == 0;
}

void prepareChip()
{
    Serial.println("Si4737 setup sequence:");

    Serial.print("Set RSTb LOW, set power HIGH... ");
    pinMode(SI4737_PIN_RESET, OUTPUT);
    pinMode(POWER_PIN, OUTPUT);
    digitalWrite(SI4737_PIN_RESET, LOW);
    digitalWrite(POWER_PIN, HIGH);
    Serial.println("OK");

    Serial.print("Wait 250us for VDD and VIO to stablize... ");
    sleep(250);
    Serial.println("OK");

    Serial.print("Use default bus mode and wait 100us to stabalize... ");
    sleep(100);
    Serial.println("OK");

    Serial.print("Set RESTb HIGH and wait at least 10ns... ");
    digitalWrite(SI4737_PIN_RESET, HIGH);
    sleep(1);
    Serial.println("OK");
    Serial.println("Si4737 setup sequence complete!");
    Serial.println();
}

void loop()
{
}

