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

void Si4737::end(bool hardoff){
	sendCommand(SI4735_CMD_POWER_DOWN);
	if(hardoff) {
		//datasheet calls for 10ns, Arduino can only go as low as 3us
		delayMicroseconds(5);
		digitalWrite(_pinReset, LOW);
		if(_pinPower != SI4735_PIN_POWER_HW) digitalWrite(_pinPower, LOW);
	};
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

	setMode(mode, false, xosc); //not used yet
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

void Si4737::getRSQ(Si4737_RX_Metrics* RSQ){
	switch(_mode){
	case SI4735_MODE_FM:
		sendCommand(SI4735_CMD_FM_RSQ_STATUS, SI4735_FLG_INTACK);
		break;
	case SI4735_MODE_AM:
		sendCommand(SI4735_CMD_AM_RSQ_STATUS, SI4735_FLG_INTACK);
		break;
	case SI4735_MODE_WB:
		//nothing
		break;
	}
	//Now read the response
	getResponse(_response);

	//Pull the response data into their respecive fields
	RSQ->RSSI = _response[4];
	RSQ->SNR = _response[5];
	if(_mode == SI4735_MODE_FM){
		RSQ->PILOT = _response[3] & SI4735_STATUS_PILOT;
		RSQ->STBLEND = (_response[3] & (~SI4735_STATUS_PILOT));
		RSQ->MULT = _response[6];
		RSQ->FREQOFF = _response[7];
	}
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
	//RIC DEBUG
	Serial.print(" .. ");
	Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)1);
	//I2C runs at 100kHz when using the Wire library, 100kHz = 10us period
	//so wait 10 bit-times for something to become available.
	while(!Wire.available()) delayMicroseconds(100);
	response = Wire.read();

	//Serial.print(" gs");
	//Serial.print(response);

	return response;
}

word Si4737::getProperty(word property){
	sendCommand(SI4735_CMD_GET_PROPERTY, 0x00, highByte(property),
		lowByte(property));
	getResponse(_response);

	return word(_response[2], _response[3]);
}

word Si4737::getFrequency(bool* valid){
	word frequency;

	switch(_mode){
	case SI4735_MODE_FM:
		sendCommand(SI4735_CMD_FM_TUNE_STATUS);
		break;
	case SI4735_MODE_AM:
		sendCommand(SI4735_CMD_AM_TUNE_STATUS);
		break;
	case SI4735_MODE_WB:
		sendCommand(SI4735_CMD_WB_TUNE_STATUS);
		break;
	}
	getResponse(_response);
	frequency = word(_response[2], _response[3]);

	if(valid) *valid = (_response[1] & SI4735_STATUS_VALID);
	return frequency;
}

void Si4737::setProperty(word property, word value){
	sendCommand(SI4735_CMD_SET_PROPERTY, 0x00, highByte(property),
		lowByte(property), highByte(value), lowByte(value));
	//Datasheet states SET_PROPERTY completes 10ms after sending the command
	//irrespective of CTS coming up earlier than that
	delay(10);
}

void Si4737::setFrequency(long frequency){
	switch(_mode){
	case SI4735_MODE_FM:
		sendCommand(SI4735_CMD_FM_TUNE_FREQ, 0x00, highByte(frequency),
			lowByte(frequency));
		break;
	case SI4735_MODE_AM:
		sendCommand(SI4735_CMD_AM_TUNE_FREQ, 0x00, highByte(frequency),
			lowByte(frequency), 0x00,
			(0x00));
		break;
	case SI4735_MODE_WB: //Input should be in 10 KHz block
		frequency = frequency * 2 / 5; //Per AN332, signal is in 2500 KHz blocks.
		sendCommand(SI4735_CMD_WB_TUNE_FREQ, 0x00, highByte(frequency),
			lowByte(frequency), 0x00,
			(0x00));
		break;
	}
	completeTune();
}

void Si4737::setMode(byte mode, bool powerdown, bool xosc){
	if(powerdown) end(false);
	_mode = mode;

	Serial.print("Set mode to ");
	Serial.print(mode);

	switch(_mode){
	case SI4735_MODE_FM:
		sendCommand(SI4735_CMD_POWER_UP,
			SI4735_FUNC_FM,
			SI4735_OUT_ANALOG);
		break;
	case SI4735_MODE_AM:
		sendCommand(SI4735_CMD_POWER_UP,
			((_pinGPO2 == SI4735_PIN_GPO2_HW) ? 0x00 :
			SI4735_FLG_GPO2IEN) |
			(xosc ? SI4735_FLG_XOSCEN : 0x00) | SI4735_FUNC_AM,
			SI4735_OUT_ANALOG);
		break;
	case SI4735_MODE_WB: //Yes, the variable is wrong. Eventually I'll switch everything over to SI4737
		sendCommand(SI4735_CMD_POWER_UP, SI4735_FUNC_WB, SI4735_OUT_ANALOG);
		break;
	}

	//Configure GPO lines to maximize stability (see datasheet for discussion)
	//No need to do anything for GPO1 if using SPI
	//No need to do anything for GPO2 if using interrupts
	//	sendCommand(SI4735_CMD_GPIO_CTL, (_i2caddr ? SI4735_FLG_GPO1OEN : 0x00) |
	//		((_pinGPO2 == SI4735_PIN_GPO2_HW) ?
	//SI4735_FLG_GPO2OEN : 0x00));
	//Set GPO2 high if using interrupts as Si4735 has a LOW active INT line
	//if(_pinGPO2 != SI4735_PIN_GPO2_HW)
	//	sendCommand(SI4735_CMD_GPIO_SET, SI4735_FLG_GPO2LEVEL);

	//Enable end-of-seek and RDS interrupts, if we're actually using interrupts
	//TODO: write interrupt handlers for STCINT and RDSINT
	//if(_pinGPO2 != SI4735_PIN_GPO2_HW)
	//	setProperty(
	//	SI4735_PROP_GPO_IEN,
	//	word(0x00, ((_mode == SI4735_MODE_FM) ? SI4735_FLG_RDSIEN : 0x00) |
	//	SI4735_FLG_STCIEN));
}

void Si4737::seekUp(bool wrap){
	switch(_mode){
	case SI4735_MODE_FM:
		sendCommand(SI4735_CMD_FM_SEEK_START,
			(SI4735_FLG_SEEKUP |
			(wrap ? SI4735_FLG_WRAP : 0x00)));
		break;
	case SI4735_MODE_AM:
		sendCommand(SI4735_CMD_AM_SEEK_START,
			(SI4735_FLG_SEEKUP | (wrap ? SI4735_FLG_WRAP : 0x00)),
			0x00, 0x00, 0x00, 0x00);
		break;
		//Sadly there is no seek for WB per AN332
	}
	completeTune();
}

void Si4737::seekDown(bool wrap){
	switch(_mode){
	case SI4735_MODE_FM:
		sendCommand(SI4735_CMD_FM_SEEK_START,
			(wrap ? SI4735_FLG_WRAP : 0x00));
		break;
	case SI4735_MODE_AM:
		sendCommand(SI4735_CMD_AM_SEEK_START,
			(wrap ? SI4735_FLG_WRAP : 0x00), 0x00, 0x00, 0x00, 0x00);
		break;
	}
	completeTune();
}

void Si4737::enableRDS(void){
	//Enable and configure RDS reception
	if(_mode == SI4735_MODE_FM) {
		setProperty(SI4735_PROP_FM_RDS_INT_SOURCE, word(0x00,
			SI4735_FLG_RDSRECV));
		setProperty(SI4735_PROP_FM_RDS_INT_FIFO_COUNT, word(0x00, 0x01));
		setProperty(SI4735_PROP_FM_RDS_CONFIG, word(SI4735_FLG_BLETHA_35 |
			SI4735_FLG_BLETHB_35 | SI4735_FLG_BLETHC_35 |
			SI4735_FLG_BLETHD_35, SI4735_FLG_RDSEN));
	};
}

void Si4737::waitForInterrupt(byte which){
	while(!(getStatus() > 0 & which)){
		//Balance being snappy with hogging the chip
		delay(125);
		sendCommand(SI4735_CMD_GET_INT_STATUS);
	}
}

void Si4737::completeTune(void) {
	waitForInterrupt(SI4735_STATUS_STCINT);
	//Make future off-to-on STCINT transitions visible
	switch(_mode){
	case SI4735_MODE_FM:
		sendCommand(SI4735_CMD_FM_TUNE_STATUS, SI4735_FLG_INTACK);
		break;
	case SI4735_MODE_AM:
		sendCommand(SI4735_CMD_AM_TUNE_STATUS, SI4735_FLG_INTACK);
		break;
	case SI4735_MODE_WB:
		sendCommand(SI4735_CMD_WB_TUNE_STATUS, SI4735_FLG_INTACK);
		break;
	}
	//  if(_mode == SI4735_MODE_FM) enableRDS();
}