/*
Name:		BasicSi4737_I2CExample.ino
Created:	9/28/2015 3:00:31 PM
Author:	Sean
*/

#include <Wire.h>

// change this to 0 if running at 5v, 1 if running at 3.3v
#define CLOCK_SHIFT_5V 0
#define CLOCK_SHIFT_3_3V 1
#define CLOCK_SHIFT CLOCK_SHIFT_3_3V
#define BASE_CLOCK 16000000
#define REAL_CLOCK BASE_CLOCK >> CLOCK_SHIFT

#define SI4737_BUS_ADDRESS 0x11
#define POWER_PIN 8
#define SI4737_PIN_RESET A0
#define GPO2_INTERUPT_PIN 3
#define CLOCK_PIN 9

#define POWERUP_CTS_INT_EN 0x80
#define POWERUP_GPO2_EN 0x40
#define POWERUP_PATCH  0x20
#define POWERUP_X_OSC_EN 0x10
#define POWERUP_FUNC_FM_RCV 0x00
#define POWERUP_FUNC_WB_RCV 0x03
#define POWERUP_FUNC_QUERY_LIB_ID 0x0F

#define POWERUP_OPMODE_RDS_ONLY 0x00
#define POWERUP_OPMODE_ANALOG 0x05
#define POWERUP_OPMODE_DIGITAL_ON_ANALOG_PINS 0x0B
#define POWERUP_OPMODE_DIGITAL 0xB0


byte ARGS[7] = { 0,0,0,0,0,0,0 };
byte RESP[15] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

bool ready = true, CTS = true, needCTS = false, STC = true, needSTC = false;
int step = -1, expResp = 0, clockPinState = LOW;

void prepareChip();
void wait(int);
void sleep(int);
bool sendCommand(const char*, const byte, const size_t);
void printBuffer(const char*, const size_t, const byte*, int = 16);
void GPO2InteruptHandler();
void getStatus();
void advanceStep();
void executeStep();
int translateWB(int);
void timer1_setup(byte, int, byte, byte, byte);

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

    attachInterrupt(digitalPinToInterrupt(GPO2_INTERUPT_PIN), GPO2InteruptHandler, FALLING);

    Wire.begin();
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
        getStatus();
        if (!needCTS || CTS)
        {
            advanceStep();
            executeStep();
        }
    }
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
        }
    }
}

void advanceStep()
{
    if (!needSTC || STC)
    {
        ++step;
    }
    else
    {
        sleep(1000);
    }

    needCTS = false;
    CTS = false;
    needSTC = false;
    STC = false;
    expResp = 0;
}

void executeStep()
{
    int f;
    Serial.print("\nstep ");
    Serial.print(step);
    Serial.print(": ");
    switch (step)
    {
    case 0:
        // do the powerup shuffle
        needCTS = true;
        ARGS[0] = POWERUP_CTS_INT_EN | POWERUP_GPO2_EN | POWERUP_FUNC_FM_RCV;
        ARGS[1] = POWERUP_OPMODE_ANALOG;
        sendCommand("POWER_UP", 0x01, 2);
        break;

    case 1:
        // enable GPIO pins, enable all to avoid excessive current consumption
        // due to ascillation.
        needCTS = true;
        ARGS[0] = 0x0E;
        sendCommand("GPIO_CTL", 0x80, 1);
        break;

    case 2:
        // Set the interupt sources
        ARGS[0] = 0x00; // always 0
        ARGS[1] = 0x00;
        ARGS[2] = 0x01;
        ARGS[3] = 0x00;
        ARGS[4] = 0xC9;
        sendCommand("SET_PROPERTY: GPO_IEN", 0x12, 5);
        // always delay 10ms after SET_PROPERTY. It has no CTS response,
        // but the 10ms is guaranteed.
        delay(10);
        ready = true;
        break;

    case 3:
        // Set the reference clock prescale
        ARGS[0] = 0x00; // always 0
        ARGS[1] = 0x02;
        ARGS[2] = 0x02;
        ARGS[3] = 0x00;
        ARGS[4] = 0x01;
        sendCommand("SET_PROPERTY: REFCLK_PRESCALE", 0x12, 5);
        // always delay 10ms after SET_PROPERTY. It has no CTS response,
        // but the 10ms is guaranteed.
        delay(10);
        ready = true;
        break;

    case 4:
        // Set the reference clock frequency
        ARGS[0] = 0x00; // always 0
        ARGS[1] = 0x02;
        ARGS[2] = 0x01;
        ARGS[3] = (31250 & 0xFF00) >> 8;
        ARGS[4] = (31250 & 0xFF);
        sendCommand("SET_PROPERTY: REFCLK_FREQ", 0x12, 5);
        // always delay 10ms after SET_PROPERTY. It has no CTS response,
        // but the 10ms is guaranteed.
        delay(10);
        ready = true;
        break;

    case 5:
        sleep(500);
        // Get current tuning
        needCTS = true;
        ARGS[0] = 0x01;
        //expResp = 5;
        //sendCommand("WB_TUNE_STATUS", 0x52, 1);
        expResp = 7;
        sendCommand("FM_TUNE_STATUS", 0x22, 1);
        break;

    case 6:
        Serial.print("Station: ");
        Serial.println(highByte(RESP[0]) | RESP[1]);
        needCTS = true;
        // Tune to WB 162.450
        //f = translateWB(162450);
        // Tune to FM 98.70
        f = 9870;
        ARGS[0] = 0x00; // always 0
        ARGS[1] = (f & 0xff00) >> 8;
        ARGS[2] = (f & 0x00ff);
        //sendCommand("WB_TUNE_FREQ: 162.450", 0x50, 3);
        ARGS[3] = 0x00; // automatically set the tuning cap
        sendCommand("FM_TUNE_FREQ: 98.7", 0x20, 4);
        break;

    case 7:
        // Check interupt status
        needCTS = true;
        needSTC = true;
        sendCommand("GET_INT_STATUS", 0x14, 0);
        break;

    case 8:
        needCTS = true;
        ARGS[0] = 0x01;
        expResp = 7;
        sendCommand("FM_TUNE_STATUS", 0x22, 1);
        break;

    case 9:
        Serial.print("Station: ");
        Serial.println((((int)RESP[1]) << 8) | RESP[2]);
        Serial.print("Waiting a few seconds");
        wait(5);
        Serial.println(" OK");
        ready = true;
        break;

    case 10:
        // seek up
        needCTS = true;
        ARGS[0] = 0x08;
        sendCommand("FM_SEEK_START: UP", 0x21, 1);
        break;

    case 11:
        // Check interupt status
        needCTS = true;
        needSTC = true;
        sendCommand("GET_INT_STATUS", 0x14, 0);
        break;

    case 12:
        needCTS = true;
        ARGS[0] = 0x01;
        expResp = 7;
        sendCommand("FM_TUNE_STATUS", 0x22, 1);
        break;

    case 13:
        Serial.print("Station: ");
        Serial.println((((int)RESP[1]) << 8) | RESP[2]);
        Serial.print("Waiting a few seconds");
        wait(5);
        Serial.println(" OK");
        ready = true;
        break;

    case 14:
        needCTS = true;
        ARGS[0] = 0x01;
        expResp = 7;
        sendCommand("FM_TUNE_STATUS", 0x22, 1);
        break;

    case 15:
        Serial.print("Station: ");
        Serial.println((((int)RESP[1]) << 8) | RESP[2]);
        Serial.print("Waiting a few seconds");
        wait(5);
        Serial.println(" OK");
        ready = true;
        break;

    case 16:
        Serial.print("Station: ");
        Serial.println((((int)RESP[1]) << 8) | RESP[2]);
        Serial.println("All done");
        sendCommand("POWER_DOWN", 0x11, 0);
        break;
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

bool sendCommand(const char* name, const byte cmd, const size_t numArgs)
{
    Serial.print("CMD ");
    printBuffer(name, numArgs, ARGS);
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

    Serial.print("Set RESTb HIGH and wait at least 10ns... ");
    digitalWrite(SI4737_PIN_RESET, HIGH);
    sleep(1);
    Serial.println("OK");
    Serial.println("Si4737 setup sequence complete!");
    Serial.println();
}

ISR(TIMER1_COMPA_vect)
{
    digitalWrite(CLOCK_PIN, clockPinState = !clockPinState);
}

int translateWB(int wb)
{
    return 64960 + (wb - 162400) / 25;
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
