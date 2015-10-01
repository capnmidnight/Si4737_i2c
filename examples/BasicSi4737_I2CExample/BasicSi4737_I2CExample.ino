/*
Name:		BasicSi4737_I2CExample.ino
Created:	9/28/2015 3:00:31 PM
Author:	Sean
*/

#include <Wire.h>

#define SI4737_BUS_ADDRESS 0x11
#define POWER_PIN 8
#define SI4737_PIN_RESET A0
#define CLOCK_PIN 9
#define CLOCK_SHIFT 1
#define ARDUINO_PWM_FREQ 490 >> CLOCK_SHIFT
#define GPO2_INTERUPT_PIN 2

#define POWERUP_CTS_INT_EN 0x80
#define POWERUP_GPO2_EN 0x40
#define POWERUP_PATCH  0x20
#define POWERUP_X_OSC_EN 0x10
#define POWERUP_FUNC_FM_RCV 0x00
#define POWERUP_FUNC_QUERY_LIB_ID 0x0F

#define POWERUP_OPMODE_RDS_ONLY 0x00
#define POWERUP_OPMODE_ANALOG 0x05
#define POWERUP_OPMODE_DIGITAL_ON_ANALOG_PINS 0x0B
#define POWERUP_OPMODE_DIGITAL 0xB0


byte ARGS[7] = { 0,0,0,0,0,0,0 };
byte RESP[15] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

bool ready = true, CTS = true, needCTS = false, STC = true, needSTC = false;
int step = 0, expResp = 0;

void prepareChip();
void wait(int);
void sleep(int);
bool sendCommand(const char*, const byte, const size_t);
void printBuffer(const char*, const size_t, const byte*, int = 16);
void GPO2InteruptHandler();

void setup()
{
    attachInterrupt(digitalPinToInterrupt(GPO2_INTERUPT_PIN), GPO2InteruptHandler, FALLING);
    // NOTE: if you're running on a 3.3v Arduino, you should double the baud rate
    // at which you run the SoftSerial library, but do not double it in the Serial
    // Monitor, because the lower voltage Arduino's only run at half the clock
    // rate but doesn't know it, so the SoftSerial code assumes it's running at full
    // speed, which screws up the timing.
    Serial.begin(115200 << CLOCK_SHIFT);
    Serial.println();
    Serial.println();
    Serial.println("OK");

    Wire.begin();

    prepareChip();

    //sendCommand("POWER_DOWN", 0x11, 0, 0);
}

void GPO2InteruptHandler()
{
    ready = true;
}

void loop()
{
    if (ready)
    {
        ready = false;
        bool needStatus = needCTS || needSTC;
        if (expResp > 0 || needStatus)
        {
            Serial.print("... Retrieving response...");
            Wire.requestFrom(SI4737_BUS_ADDRESS, expResp + (needStatus ? 1 : 0));
            while (Wire.available() < expResp)
            {
                Serial.print('.');
                sleep(100);
            }

            if (needStatus)
            {
                byte status = Wire.read();
                if (needCTS && (status & 0x80) != 0)
                {
                    CTS = true;
                }

                if (needSTC && (status & 0x01) != 0)
                {
                    STC = true;
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
                    Serial.print("STC ");
                }
            }

            if (expResp > 0) 
            {
                Wire.readBytes(RESP, expResp);
                printBuffer("RESP", expResp, RESP);
                expResp = 0;
            }
        }

        Serial.print("\nstep ");
        Serial.print(step);
        Serial.print(": ");
        switch (step)
        {
        case 0:
            // do the powerup shuffle
            ARGS[0] = POWERUP_CTS_INT_EN | POWERUP_GPO2_EN | POWERUP_X_OSC_EN | POWERUP_FUNC_FM_RCV;
            ARGS[1] = POWERUP_OPMODE_ANALOG;
            needCTS = true;
            sendCommand("POWER_UP", 0x01, 2);
            break;

        case 1:
            // enable GPIO pins, enable all to avoid excessive current consumption
            // due to ascillation.
            ARGS[0] = 0x0E;
            needCTS = true;
            sendCommand("GPIO_CTL", 0x80, 1);
            break;

        case 2:
            // Set the interupt sources
            ARGS[0] = 0x00; // always 0
            ARGS[1] = 0x00;
            ARGS[2] = 0x01;
            ARGS[3] = 0x00;
            ARGS[4] = 0x81;
            sendCommand("SET_PROPERTY: GPO_IEN", 0x12, 5);
            ready = true;
            break;

        case 3:
            // Tune to 97.1
            ARGS[0] = 0x00; // always 0
            ARGS[1] = (9710 & 0xff00) >> 8;
            ARGS[2] = (9710 & 0x00ff);
            ARGS[3] = 0x00; // automatically set the tuning cap
            needCTS = true;
            needSTC = true;
            sendCommand("FM_TUNE_FREQ: 97.1", 0x20, 4);
            break;

        case 4:
            Serial.print("Waiting a few seconds");
            wait(3);
            Serial.println(" OK");
            ready = true;
            break;

        case 5:
            ARGS[0] = 0x00;
            expResp = 7;
            needCTS = true;
            sendCommand("GET_PROPERTY: FM_TUNE_STATUS", 0x13, 1);
            break;

        case 6:
            printBuffer("Tune", 7, RESP);
            Serial.println("All done");
            break;
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

void printBuffer(const char* name, const size_t size, const byte* buffer, int radix)
{
    Serial.print(name);
    Serial.print('(');
    if (size > 0)
    {
        for (int i = 0; i < size; ++i)
        {
            Serial.print(buffer[i], radix);
            if (i < (size - 1))
            {
                Serial.print(", ");
            }
        }
    }
    Serial.println(')');
}

bool sendCommand(const char* name, const byte cmd, const size_t numArgs)
{
    Serial.print("CMD ");
    printBuffer(name, numArgs, ARGS);
    Serial.print("... ");
    Wire.beginTransmission(SI4737_BUS_ADDRESS);
    Wire.write(cmd);
    if (numArgs > 0)
    {
        Wire.write(ARGS, numArgs);
    }

    byte status = Wire.endTransmission(false);

#ifdef DEBUG_I2C
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
#endif
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

    Serial.print("Wait 250ms for VDD and VIO to stablize... ");
    sleep(250);
    Serial.println("OK");

    Serial.print("Use default bus mode and wait 100ms to stabalize... ");
    sleep(100);
    Serial.println("OK");


    Serial.print("Start the clock signal and wait 500ms for it to stabalize... ");
    pinMode(CLOCK_PIN, OUTPUT);
    analogWrite(CLOCK_PIN, 128);
    sleep(500);
    Serial.println("OK");

    Serial.print("Set RESTb HIGH and wait at least 10ns... ");
    digitalWrite(SI4737_PIN_RESET, HIGH);
    sleep(1);
    Serial.println("OK");
    Serial.println("Si4737 setup sequence complete!");
    Serial.println();
}

