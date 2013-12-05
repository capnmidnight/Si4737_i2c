/* 
	UWAFT Si4737 Stuff
	
	Functions will be stolen from the Si4735 library as needed
*/

//Appetizer
#include "Si4737_i2c.h"
#include "Si4737_i2c-private.h"
#include <Wire.h>

//Main Course
Si4737::Si4737(byte interface, byte pinPower, byte pinReset, byte pinGPO2,
               byte pinSEN){
    _mode = SI4735_MODE_FM;
    _pinPower = pinPower;
    _pinReset = pinReset;
    _pinGPO2 = pinGPO2;
    _pinSEN = pinSEN;
    switch(interface){
        case SI4735_INTERFACE_SPI:
            _i2caddr = 0x00;
            break;
        case SI4735_INTERFACE_I2C:
            if(_pinSEN == SI4735_PIN_SEN_HWH) _i2caddr = SI4735_I2C_ADDR_H;
            else _i2caddr = SI4735_I2C_ADDR_L;
            break;
    }
}

void Si4737::begin(byte mode, bool xosc, bool slowshifter){

	//RIC - Removing all irrelevant SPI stuff
	Serial.print("Init Si4737 at _i2caddr=");
	Serial.print(_i2caddr);
	Serial.println("");
	
    //Reset pinout config
    pinMode(_pinReset, OUTPUT);
    //pinMode((_i2caddr ? SCL : SCK), OUTPUT); Dont know what this does
	delayMicroseconds(100);
    digitalWrite(_pinReset, LOW); //Reset it and wait
    //Use the longest of delays given in the datasheet
    delayMicroseconds(100);
	/* No idea
    if(_pinPower != SI4735_PIN_POWER_HW) {
        digitalWrite(_pinPower, HIGH);
        //Datasheet calls for 250us between VIO and RESET
        delayMicroseconds(250);
    };
    digitalWrite((_i2caddr ? SCL : SCK), LOW);
	*/
    //Datasheet calls for no rising SCLK edge 300ns before RESET rising edge,
    //but Arduino can only go as low as 3us.
    delayMicroseconds(5);
    digitalWrite(_pinReset, HIGH);
    //Datasheet calls for 30ns from rising edge of RESET until GPO1/GPO2 bus
    //mode selection completes, but Arduino can only go as low as 3us.
    delayMicroseconds(5);


	//Configure the I2C hardware
	Wire.begin();
	Serial.println("Wire has begun, I2C now active");

    //setMode(mode, false, xosc); not used yet
}

void Si4737::sendCommand(byte command, byte arg1, byte arg2, byte arg3,
byte arg4, byte arg5, byte arg6, byte arg7){

  Serial.print("Si4735 CMD 0x");
  Serial.print(command, HEX);
  Serial.print(" (0x");
  Serial.print(arg1, HEX);
  Serial.print(" [");
  Serial.print(arg1, BIN);
  Serial.print("], 0x");
  Serial.print(arg2, HEX);
  Serial.print(" [");
  Serial.print(arg2, BIN);
  Serial.print("], 0x");
  Serial.print(arg3, HEX);
  Serial.print(" [");
  Serial.print(arg3, BIN);
  Serial.println("],");
  Serial.print("0x");
  Serial.print(arg4, HEX);
  Serial.print(" [");
  Serial.print(arg4, BIN);
  Serial.print("], 0x");
  Serial.print(arg5, HEX);
  Serial.print(" [");
  Serial.print(arg5, BIN);
  Serial.print("], 0x");
  Serial.print(arg6, HEX);
  Serial.print(" [");
  Serial.print(arg6, BIN);
  Serial.print("], 0x");
  Serial.print(arg7, HEX);
  Serial.print(" [");
  Serial.print(arg7, BIN);
  Serial.println("])");
  Serial.flush();

  Wire.beginTransmission(_i2caddr);
  Wire.write(command);
  Wire.write(arg1);
  Wire.write(arg2);
  Wire.write(arg3);
  Wire.write(arg4);
  Wire.write(arg5);
  Wire.write(arg6);
  Wire.write(arg7);
  Wire.endTransmission();

  byte status;
  do {
    status = getStatus();
  } 
  while(!(status & SI4735_STATUS_CTS));
}

void Si4737::getResponse(byte* response){

  Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)16);
  for(int i = 0; i < 16; i++) {
    //I2C runs at 100kHz when using the Wire library, 100kHz = 10us
    //period so wait 10 bit-times for something to become available.
    while(!Wire.available()) delayMicroseconds(100);
    response[i] = Wire.read();
  }

  Serial.print("Si4735 RSP");
  for(int i = 0; i < 4; i++) {
    if(i) Serial.print("           ");
    else Serial.print(" ");
    for(int j = 0; j < 4; j++) {
      Serial.print("0x");
      Serial.print(response[i * 4 + j], HEX);
      Serial.print(" [");
      Serial.print(response[i * 4 + j], BIN);
      Serial.print("]");
      if(j != 3) Serial.print(", ");
      else
        if(i != 3) Serial.print(",");
    }
    Serial.println("");
  }
}

byte Si4737::getStatus(void){
  byte response = 0;

    Serial.print(" .. ");
    Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)1);
    //I2C runs at 100kHz when using the Wire library, 100kHz = 10us period
    //so wait 10 bit-times for something to become available.
    while(!Wire.available()) delayMicroseconds(100);
    response = Wire.read();

  return response;
}
