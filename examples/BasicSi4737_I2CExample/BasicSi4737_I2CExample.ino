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

    Serial.print("Writing POWER_UP command... ");
    Wire.beginTransmission(SI4737_BUS_ADDRESS);
    Wire.write(0x01); // POWER_UP
    Wire.write(0xC0); // HIGH[XOSCEN], LOW[FM_RECV]
    Wire.write(0x50); // ANALOG_OUT
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

    if (status == 0)
    {
        bool clear = false;
        while (!clear)
        {
            Serial.print("Retrieve clear-to-send status..");
            Wire.requestFrom(SI4737_BUS_ADDRESS, 1);
            while (!Wire.available())
            {
                Serial.print('.');
                delay(10);
            }
            status = Wire.read();
            Serial.print(' ');
            Serial.print(status, 16);
            Serial.print(" -> ");
            if (status & 0x80)
            {
                Serial.print("clear-to-send ");
                clear = true;
            }
            if (status & 0x40)
            {
                Serial.print("ERROR ");
            }
            Serial.println();
        }
    }
}

void loop()
{
}

