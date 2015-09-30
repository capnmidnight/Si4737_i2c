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

void prepareChip();
bool translateI2CStatus(byte);
bool translateChipStatus(byte);
byte getChipStatus();
bool sendCommand(const char*, const byte, const size_t, const byte*, size_t, byte*, bool = true);
void printResponse(const char*, const size_t, const byte*, int = 16);

void setup()
{
    Wire.begin();

    // NOTE: if you're running on a 3.3v Arduino, you should double the baud rate
    // at which you run the SoftSerial library, but do not double it in the Serial
    // Monitor, because the lower voltage Arduino's only run at half the clock
    // rate but doesn't know it, so the SoftSerial code assumes it's running at full
    // speed, which screws up the timing.
    Serial.begin(115200 * 2);
    Serial.println();
    Serial.println();

    prepareChip();

    byte POWER_UP[] = { 0x01, 2, 1 };
    byte GET_REV[] = { 0x10, 0, 8 };

    byte ARGS[7] = { 0,0,0,0,0,0,0 };
    byte RESP[15] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
    ARGS[0] = 0x10;
    ARGS[1] = 0x05;
    if (sendCommand("POWER_UP", 0x01, 2, ARGS, 0, RESP)
        && sendCommand("GET_REV", 0x10, 0, ARGS, 8, RESP))
    {
        printResponse("Revision", 8, RESP);

        // get volume
        ARGS[0] = 0x00; // always 0
        ARGS[1] = 0x40;
        ARGS[2] = 0x00;
        if (sendCommand("GET_PROPERTY: VOLUME", 0x13, 3, ARGS, 3, RESP)) 
        {
            printResponse("Volume", 3, RESP);

            // set volume
            ARGS[0] = 0x00; // always 0
            ARGS[1] = 0x40;
            ARGS[2] = 0x00;
            ARGS[3] = 0x00;
            ARGS[4] = 0x50;
            if (sendCommand("SET_PROPERTY: VOLUME", 0x12, 5, ARGS, 0, RESP, false))
            {
                if (sendCommand("GET_PROPERTY: VOLUME", 0x13, 3, ARGS, 3, RESP))
                {
                    printResponse("Volume", 3, RESP);
                    if (sendCommand("POWER_DOWN", 0x11, 0, ARGS, 0, RESP))
                    {
                        Serial.println("Successfully shut down.");
                    }
                }
            }
        }
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

bool sendCommand(const char* name, const byte cmd, const size_t numArgs, const byte* args, size_t numResp, byte* resp, bool checkCTS)
{
    if (checkCTS) ++numResp;
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
    bool cmdGood = numResp > 0 ? false : true;
    // try only 3 times before giving up, we don't want to spin forever
    for (int i = 0; i < 3 && i2cGood && !cmdGood; ++i)
    {
        // always delay right away, to give the chip some time to do
        // do whatever we asked it to do.
        delay(100);
        Serial.print("Retrieving response..");
        Wire.requestFrom(SI4737_BUS_ADDRESS, numResp);
        while (Wire.available() < numResp)
        {
            Serial.print('.');
            delay(100);
        }
        if (checkCTS) 
        {
            --numResp;
            byte status = Wire.read();
            Serial.print(' ');
            Serial.print(status, 16);
            Serial.print(" -> ");

            if (status & 0x80)
            {
                Serial.print("CTS ");
                cmdGood = true;
            }

            if (status & 0x40)
            {
                Serial.print("ERROR ");
            }

            Serial.println();
        }
        Wire.readBytes(resp, numResp);
    }
    return i2cGood && cmdGood;
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
    delay(250);
    Serial.println("OK");

    Serial.print("Use default bus mode and wait 100us to stabalize... ");
    delay(100);
    Serial.println("OK");

    Serial.print("Set RESTb HIGH and wait at least 10ns... ");
    digitalWrite(SI4737_PIN_RESET, HIGH);
    delay(1);
    Serial.println("OK");
    Serial.println("Si4737 setup sequence complete!");
    Serial.println();
}

void loop()
{
}

