/*
HOW TO USE

Go to 99.9 FM: "9990g"
Switch to WB: "3m"
Go to 162.400 MHz for WB: "162400g"
Send command 0x11: "c11w"

*/

//Due to a bug in Arduino, these need to be included here too/first
#include <Wire.h>
#include <SPI.h> //Dunno what this does

//UWAFT Si4737 I2C Functions
#include <Si4737_i2c.h>

//Create our Si4737 radio
Si4737 radio(
    SI4737_INTERFACE_I2C,
    A0,
    A3,
    SI4737_PIN_GPO2_HWL,
    SI4737_PIN_SEN_HWL);

//Other variables we will use below
byte response[16], command[8], numDigits, status, mode;
char collectedDigits[17] = "", serial_in, oneHexValue[5] = "0x";
word frequency;
bool goodtune;
Si4737_RX_Metrics RSQ;

void setup() {
    // NOTE: if you're running on a 3.3v Arduino, you should double the baud rate
    // at which you run the SoftSerial library, but do not double it in the Serial
    // Monitor, because the lower voltage Arduino's only run at half the clock
    // rate but doesn't know it, so the SoftSerial code assumes it's running at full
    // speed, which screws up the timing.
    Serial.begin(115200*2);

    radio.begin(SI4737_MODE_FM);
    Serial.println("ino code - Radio Powered up, FM mode");
    delay(100);                          // Wait for Boot 

    radio.seekUp();

}

void loop()
{
    //Wait to do something until a character is received on the serial port.
    if (Serial.available() > 0) {
        //Copy the incoming character to a variable and fold case
        serial_in = toupper(Serial.read());
        //Depending on the incoming character, decide what to do.
        switch (serial_in) {
        case 'U':
            Serial.println("Seeking up");
            radio.seekUp();
            break;
        case 'D':
            Serial.println("Seeking Down");
            radio.seekDown();
            break;
        case 'M': //Switch to Mode
            if (atoi(collectedDigits) == 1) {
                Serial.println("Go to FM");
                radio.setMode(SI4737_MODE_FM);
            }
            else if (atoi(collectedDigits) == 2) {
                Serial.println("Go to AM");
                radio.setMode(SI4737_MODE_AM);
            }
            else if (atoi(collectedDigits) == 3) {
                Serial.println("Go to WB");
                radio.setMode(SI4737_MODE_WB); //SI4737 does not have WB. This variable name is wrong.
            }
            else {
                Serial.println("Mode Not Recognized ");
                Serial.println(atoi(collectedDigits));
            }
            clcCmd();
        case 'G': //Go to frequency. 
                  //Frequency format is in accordance with AN332
                  //except for WB, which is same as FM for this implementation (10KHz block)
            radio.setFrequency(atol(collectedDigits)); //Use atol for long, cuz WB is long
            clcCmd();
            break;
        case 'V': //Set Volume, 0 to 63
            radio.setVolume(atoi(collectedDigits));
            clcCmd();
            break;
        case 'Q':
            radio.getRSQ(&RSQ);
            Serial.println(F("Signal quality metrics {"));
            Serial.print(F("RSSI = "));
            Serial.print(RSQ.RSSI);
            Serial.println("dBuV");
            Serial.print("SNR = ");
            Serial.print(RSQ.SNR);
            Serial.println("dB");
            if (radio.getMode() == SI4737_MODE_FM) {
                Serial.println((RSQ.PILOT ? "Stereo" : "Mono"));
                Serial.print(F("f Offset = "));
                Serial.print(RSQ.FREQOFF);
                Serial.println("kHz");
            }
            Serial.println("}");
            Serial.flush();
            break;
        case 'P':
            //Get the latest response from the radio.
            radio.getResponse(response);
            //Print all 16 bytes in the response to the terminal.      
            Serial.print(F("SI4737 RSP"));
            for (int i = 0; i < 4; i++) {
                if (i) Serial.print(F("           "));
                else Serial.print(" ");
                for (int j = 0; j < 4; j++) {
                    Serial.print("0x");
                    Serial.print(response[i * 4 + j], HEX);
                    Serial.print(" [");
                    Serial.print(response[i * 4 + j], BIN);
                    Serial.print("]");
                    if (j != 3) Serial.print(", ");
                    else
                        if (i != 3) Serial.print(",");
                };
                Serial.println("");
            };
            Serial.flush();
            break;
        case 'S':
            status = radio.getStatus();
            Serial.print(F("\nSI4737 STS 0x"));
            Serial.print(status, HEX);
            Serial.print(" [");
            Serial.print(status, BIN);
            Serial.println("]");
            Serial.flush();
            break;
        case 'F':
            frequency = radio.getFrequency(&goodtune);
            mode = radio.getMode();
            Serial.print(F("Currently tuned to "));
            switch (mode) {
            case SI4737_MODE_FM:
                Serial.print(frequency / double(100));
                Serial.println(F(" MHz FM"));
                break;
            case SI4737_MODE_AM:
                Serial.print(frequency);
                Serial.print(" kHz AM");
                break;
            case SI4737_MODE_WB:
                Serial.print(frequency / double(400));
                Serial.print(" MHz WB");
                break;
            }
            if (goodtune) Serial.println(F("* Valid tune"));
            Serial.flush();
            break;
        case 'X': //Clears all pending commands
            clcCmd();
            Serial.println(" Cmd Cleared ");
            break;
        case 'C': //Send command
                  //Collect Commands
            clcCmd();
            Serial.flush();
            Serial.println(F("Enter Command: "));
            while (serial_in != 'W') {
                serial_in = toupper(Serial.read());
                if ((serial_in >= '0' && serial_in <= '9') ||
                    (serial_in >= 'A' && serial_in <= 'F'))
                    if (strlen(collectedDigits) < 16) {
                        strncat(collectedDigits, &serial_in, 1);
                        //Echo the command string as it is composed
                        Serial.print(serial_in);
                    }
                    else Serial.println(F("Too many hex digits, 16 maximum!"));
                    Serial.flush();
            }

            //Send Commands
            numDigits = strlen(collectedDigits);
            //Silently ignore empty lines, this also gives us CR & LF support
            if (numDigits) {
                if (numDigits % 2 == 0) {
                    memset(command, 0x00, 8);
                    for (int i = 0; i < (numDigits / 2); i++) {
                        strncpy(&oneHexValue[2], &collectedDigits[i * 2], 2);
                        command[i] = strtoul(oneHexValue, NULL, 16);
                    }
                    //End command string echo
                    Serial.println("");
                    //Send the current command to the radio.
                    radio.sendCommand(command[0], command[1], command[2],
                        command[3], command[4], command[5],
                        command[6], command[7]);
                }
                else Serial.println(F("Odd number of hex digits, need even!"));
            }

            //Cleanup
            clcCmd();
            break;
        case '?':
            Serial.println(F("Available commands:"));
            Serial.println(F("* 0-z    - arguments"));
            Serial.println(F("* g      - Go to frequency (per argument)"));
            Serial.println(F("* m      - set Mode (per argument. 1=FM 2=AM 3=WB)"));
            Serial.println(F("* v      - set Voulme (per argument)"));
            Serial.println(F("* d/u    - seek Up or Down"));
            Serial.println(F("* f      - display Frequency"));
            Serial.println(F("* p      - display resPonse (long read)"));
            Serial.println(F("* s      - display Status (short read)"));
            Serial.println(F("* x      - flush (empty) command string and start over"));
            Serial.println(F("* c      - Compose command, 0 to F, at most 16 hex digits (forming an 8 byte"));
            Serial.println(F("           command) are accepted"));
            Serial.println(F("* w      - Write (send) command"));
            Serial.println(F("* ?      - display this list"));
            Serial.flush();
            break;
            //If we get any other character
            //copy it to the command string.
        default:
            strncat(collectedDigits, &serial_in, 1);
            //Echo the command string as it is composed
            Serial.print(serial_in);
            Serial.flush();
            break;
        }
    }
}

void clcCmd() {
    Serial.flush();
    collectedDigits[0] = '\0'; //Clears command for next round
}


