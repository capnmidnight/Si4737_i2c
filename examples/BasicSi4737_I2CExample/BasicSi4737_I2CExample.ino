/*
Name:		BasicSi4737_I2CExample.ino
Created:	9/28/2015 3:00:31 PM
Author:	Sean
*/

#include <Wire.h>

#define CLOCK_SHIFT_5V 0
#define CLOCK_SHIFT_3_3V 1
#define CLOCK_SHIFT CLOCK_SHIFT_3_3V

#define USE_SOFTWARE_CLOCK

#define MAX_NUM_ARGS 7

#define GPO2_INTERUPT_PIN_OPT_1 2
#define GPO2_INTERUPT_PIN_OPT_2 3
#define GPO2_INTERUPT_PIN GPO2_INTERUPT_PIN_OPT_1

#define SI4737_BUS_ADDRESS 0x11
#define POWER_PIN 8
#define SI4737_PIN_RESET A1
#define CLOCK_PIN A2

byte RESP[15] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

bool interruptReceived = true, CTS = true, needCTS = false, STC = true, needSTC = false;
int expResp = 0, clockPinState = LOW;

void(*currentStep)(void);
void(*nextStep)(void);

void prepareChip();
void wait(int);
void sleep(int);
void printBuffer(const char*, const size_t, const byte*, int = 16);
void GPO2InteruptHandler();
void getStatus();
void advanceStep();
int translateWB(int);
void timer1_setup(byte, int, byte, byte, byte);

void GPO2InteruptHandler()
{
    interruptReceived = true;
}

void loop()
{
    if (interruptReceived)
    {
        interruptReceived = false;
        getStatus();
        if (!needCTS || CTS)
        {
            advanceStep();
            (*currentStep)();
        }
    }
}

void advanceStep()
{
    if (!needSTC || STC)
    {
        currentStep = nextStep;
        nextStep = 0;
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

////////////////////////////////////////////////////////////
// CHIP COMS
////////////////////////////////////////////////////////////

void transmitCommand(const char* name, byte cmd, int responseLength, bool ctsExpected, bool stcExpected, int arg0 = -1, int arg1 = -1, int arg2 = -1, int arg3 = -1, int arg4 = -1, int arg5 = -1, int arg6 = -1)
{
    needCTS = ctsExpected;
    needSTC = stcExpected;
    if (!needCTS && !needSTC)
    {
        interruptReceived = true;
    }
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

    int numArgs = 0;
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
        Serial.println();
    }
}

void printStation()
{
    Serial.print("Station: ");
    Serial.println(makeWord(RESP[1], RESP[2]));
    Serial.print("Waiting a few seconds");
    wait(5);
    Serial.println(" OK");
    interruptReceived = true;
}

////////////////////////////////////////////////////////////
// CHIP API COMMANDS
////////////////////////////////////////////////////////////

/*
Initiates the boot process to move the device from powerdown to powerup mode. The boot can occur from internal
device memory or a system controller downloaded patch. To confirm that the patch is compatible with the internal
device library revision, the library revision should be confirmed by issuing the POWER_UP command with
FUNC = 15 (query library ID). The device returns the response, including the library revision, and then moves into
powerdown mode. The device can then be placed in powerup mode by issuing the POWER_UP command with
FUNC = 0 (FM Receive) and the patch may be applied (See Section "7.2. Powerup from a Component Patch" on
page 216).

The POWER_UP command configures the state of ROUT (pin 13, Si474x pin 15) and LOUT (pin 14, Si474x pin
16) for analog audio mode and GPO2/INT (pin 18, Si474x pin 20) for interrupt operation. For the
Si4705/21/31/35/37/39/84/85-B20, the POWER_UP command also configures the state of GPO3/DCLK (pin 17,
Si474x pin 19), DFS (pin 16, Si474x pin 18), and DOUT (pin 15, Si474x pin 17) for digital audio mode. The
command configures GPO2/INT interrupts (GPO2OEN) and CTS interrupts (CTSIEN). If both are enabled,
GPO2/INT is driven high during normal operation and low for a minimum of 1 µs during the interrupt. The CTSIEN
bit is duplicated in the GPO_IEN property. The command is complete when the CTS bit (and optional interrupt) is
set.

Note: To change function (e.g. FM RX to AM RX or FM RX to FM TX), issue POWER_DOWN command to stop current function;
then, issue POWER_UP to start new function.

Note: Delay at least 500 ms between powerup command and first tune command to wait for the oscillator to stabilize if
XOSCEN is set and crystal is used as the RCLK.

Available in: All

Response Bytes: None (FUNC != QUERY_LIBRARY_ID), Seven (FUNC = QUERY_LIBRARY_ID)
*/
void powerUp(byte funcMode, bool analogOutput = true, bool digitalOutput = false, bool enableCTSInterrupt = true, bool enableGP02 = true, bool enableXOSC = false, bool enablePatch = false)
{
#define POWER_UP 0x01
#define POWER_UP_CTS_INT_EN 0x80
#define POWER_UP_GPO2_EN 0x40
#define POWER_UP_PATCH  0x20
#define POWER_UP_X_OSC_EN 0x10
#define POWER_UP_FUNC_FM_RCV 0x00
#define POWER_UP_FUNC_WB_RCV 0x03
#define POWER_UP_FUNC_QUERY_LIB_ID 0x0F
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

    transmitCommand(
        "POWER_UP",
        POWER_UP, 0, true, false,
        options | funcMode,
        outputMode);
}

/*
Returns the part number, chip revision, firmware revision, patch revision and component revision numbers.The
command is complete when the CTS bit(and optional interrupt) is set.This command may only be sent when in
powerup mode.

Available in : All

Response bytes : Fifteen(Si4705 / 06 only), Eight(Si4704 / 2x / 3x / 4x)
*/
void getRevision(bool isSI4705or06 = false)
{
#define GET_REV 0x10

    transmitCommand(
        "GET_REV",
        GET_REV, isSI4705or06 ? 15 : 8, true, false);
}

/*
Moves the device from powerup to powerdown mode. The CTS bit (and optional interrupt) is set when it is safe to
send the next command. This command may only be sent when in powerup mode. Note that only the POWER_UP
command is accepted in powerdown mode. If the system controller writes a command other than POWER_UP
when in powerdown mode, the device does not respond. The device will only respond when a POWER_UP
command is written. GPO pins are powered down and not active during this state. For optimal power down
current, GPO2 must be either internally driven low through GPIO_CTL command or externally driven low.

Note: In FMRX component 1.0, a reset is required when the system controller writes a command other than POWER_UP
when in powerdown mode.

Note: The following describes the state of all the pins when in powerdown mode:
GPIO1, GPIO2, and GPIO3 = 0
ROUT, LOUT, DOUT, DFS = HiZ

Available in: All
*/
void powerDown()
{
#define POWER_DOWN 0x11

    transmitCommand(
        "POWER_DOWN",
        POWER_DOWN, 0, true, false);
}

/*
Sets a property shown in Table 9, “FM/RDS Receiver Property Summary,” on page 56. The CTS bit (and optional
interrupt) is set when it is safe to send the next command. This command may only be sent when in powerup
mode. See Figure 29, “CTS and SET_PROPERTY Command Complete tCOMP Timing Model,” on page 226 and
Table 46, “Command Timing Parameters for the FM Receiver,” on page 228.

Available in: All
*/
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

/*
Gets a property as shown in AN332: Table 9, “FM/RDS Receiver Property Summary,” on page 56. The CTS bit (and
optional interrupt) is set when it is safe to send the next command. This command may only be sent when in
powerup mode.

Available in: All

Response bytes: Three
*/
void getProperty(const char* name, uint16_t propertyNumber)
{
#define GET_PROPERTY 0x13

    Serial.print("GET_PROPERTY: ");
    transmitCommand(name,
        GET_PROPERTY, 3, true, false,
        0x00, // always 0
        highByte(propertyNumber), lowByte(propertyNumber));
}

/*
Updates bits 6:0 of the status byte. This command should be called after any command that sets the STCINT,
RDSINT, or RSQINT bits. When polling this command should be periodically called to monitor the STATUS byte,
and when using interrupts, this command should be called after the interrupt is set to update the STATUS byte. The
CTS bit (and optional interrupt) is set when it is safe to send the next command. This command may only be set
when in powerup mode.

Available in: All
*/
void getInterruptStatus()
{
#define GET_INT_STATUS 0x14

    transmitCommand(
        "GET_INT_STATUS",
        GET_INT_STATUS, 0, true, true);
}

/*
Sets the FM Receive to tune a frequency between 64 and 108 MHz in 10 kHz units. The CTS bit (and optional
interrupt) is set when it is safe to send the next command. The ERR bit (and optional interrupt) is set if an invalid
argument is sent. Note that only a single interrupt occurs if both the CTS and ERR bits are set. The optional STC
interrupt is set when the command completes. The STCINT bit is set only after the GET_INT_STATUS command is
called. This command may only be sent when in powerup mode. The command clears the STC bit if it is already
set. See Figure 28, “CTS and STC Timing Model,” on page 226 and Table 46, “Command Timing Parameters for
the FM Receiver,” on page 228.

FM: LO frequency is 128 kHz above RF for RF frequencies < 90 MHz and 128 kHz below RF for RF frequencies >
90 MHz. For example, LO frequency is 80.128 MHz when tuning to 80.00 MHz.

Note: For FMRX components 2.0 or earlier, tuning range is 76–108 MHz.

Note: Fast bit is supported in FMRX components 4.0 or later.

Note: Freeze bit is supported in FMRX components 4.0 or later.

Available in: All
*/
void setFMTuneFrequency(int f, bool freezeMetrics = false, bool fastTune = false, byte antennaCapValue = 0)
{
#define FM_TUNE_FREQ 0x20
#define FM_TUNE_FREQ_FREEZE 0x02
#define FM_TUNE_FREQ_FAST 0x01

    byte options = 0;
    if (freezeMetrics) options |= FM_TUNE_FREQ_FREEZE;
    if (fastTune) options |= FM_TUNE_FREQ_FAST;

    transmitCommand(
        "FM_TUNE_FREQ: 88.5",
        FM_TUNE_FREQ, 0, true, false,
        options,
        highByte(f),
        lowByte(f),
        antennaCapValue);
}

/*
Begins searching for a valid frequency. Clears any pending STCINT or RSQINT interrupt status. The CTS bit (and
optional interrupt) is set when it is safe to send the next command. RSQINT status is only cleared by the RSQ
status command when the INTACK bit is set. The ERR bit (and optional interrupt) is set if an invalid argument is
sent. Note that only a single interrupt occurs if both the CTS and ERR bits are set. The optional STC interrupt is set
when the command completes. The STCINT bit is set only after the GET_INT_STATUS command is called. This
command may only be sent when in powerup mode. The command clears the STCINT bit if it is already set. See
Figure 28, “CTS and STC Timing Model,” on page 226 and Table 46, “Command Timing Parameters for the FM
Receiver,” on page 228.

Available in: All
*/
void seekFM(bool seekUp, bool wrap)
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

/*
Returns the status of FM_TUNE_FREQ or FM_SEEK_START commands. The command returns the current
frequency, RSSI, SNR, multipath, and the antenna tuning capacitance value (0-191). The command clears the
STCINT interrupt bit when INTACK bit of ARG1 is set. The CTS bit (and optional interrupt) is set when it is safe to
send the next command. This command may only be sent when in powerup mode.

Available in: All

Response bytes: Seven
*/
void getFMTuneStatus(bool cancelSeek = false, bool ackSTC = true)
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

/*
Returns status information about the received signal quality. The commands returns the RSSI, SNR, frequency
offset, and stereo blend percentage. It also indicates valid channel (VALID), soft mute engagement (SMUTE), and
AFC rail status (AFCRL). This command can be used to check if the received signal is above the RSSI high
threshold as reported by RSSIHINT, or below the RSSI low threshold as reported by RSSILINT. It can also be used
to check if the signal is above the SNR high threshold as reported by SNRHINT, or below the SNR low threshold as
reported by SNRLINT. For the Si4706/4x, it can be used to check if the detected multipath is above the multipath
high threshold as reported by MULTHINT, or below the multipath low threshold as reported by MULTLINT. If the
PILOT indicator is set, it can also check whether the blend has crossed a threshold as indicated by BLENDINT.
The command clears the RSQINT, BLENDINT, SNRHINT, SNRLINT, RSSIHINT, RSSILINT, MULTHINT, and
MULTLINT interrupt bits when INTACK bit of ARG1 is set. The CTS bit (and optional interrupt) is set when it is safe
to send the next command. This command may only be sent when in powerup mode.

Available in: All

Response bytes: Seven
*/

void getFMRSQStatus(bool ackInterrupts = true)
{
#define FM_RSQ_STATUS 0x23
#define FM_RSQ_STATUS_INTACK 0x01

    byte options = 0;
    if (ackInterrupts) options |= FM_RSQ_STATUS_INTACK;

    transmitCommand(
        "FM_RSQ_STATUS",
        FM_RSQ_STATUS, 7, true, false,
        options);
}

/*
Returns RDS information for current channel and reads an entry from the RDS FIFO. RDS information includes
synch status, FIFO status, group data (blocks A, B, C, and D), and block errors corrected. This command clears
the RDSINT interrupt bit when INTACK bit in ARG1 is set and, if MTFIFO is set, the entire RDS receive FIFO is
cleared (FIFO is always cleared during FM_TUNE_FREQ or FM_SEEK_START). The CTS bit (and optional
interrupt) is set when it is safe to send the next command. This command may only be sent when in power up
mode. The FIFO size is 25 groups for FMRX component 2.0 or later, and 14 for FMRX component 1.0.

Notes:
1. FM_RDS_STATUS is supported in FMRX component 2.0 or later.
2. MTFIFO is not supported in FMRX component 2.0.

Available in: Si4705/06, Si4721, Si474x, Si4731/35/37/39, Si4785

Response bytes: Twelve
*/

void getFMRDSStatus(bool statusOnly = false, bool emptyFIFO = false, bool ackRDSInterrupt = true)
{
#define FM_RDS_STATUS 0x24
#define FM_RDS_STATUS_STATUSONLY 0x04
#define FM_RDS_STATUS_MTFIFO 0x02
#define FM_RDS_STATUS_INTACK 0x01

    byte options = 0;
    if (statusOnly) options |= FM_RDS_STATUS_STATUSONLY;
    if (emptyFIFO) options |= FM_RDS_STATUS_MTFIFO;
    if (ackRDSInterrupt) options |= FM_RDS_STATUS_INTACK;

    transmitCommand(
        "FM_RDS_STATUS", 
        FM_RDS_STATUS, 12, true, false, 
        options);
}

/*
Returns the AGC setting of the device. The command returns whether the AGC is enabled or disabled and it
returns the LNA Gain index. This command may only be sent when in powerup mode.

Available in: All

Response bytes: Two
*/
void getFMAGCStatus()
{
#define FM_AGC_STATUS 0x27

    transmitCommand(
        "FM_AGC_STATUS",
        FM_AGC_STATUS, 2, true, false);
}

/*
Overrides AGC setting by disabling the AGC and forcing the LNA to have a certain gain that ranges between 0
(minimum attenuation) and 26 (maximum attenuation). This command may only be sent when in powerup mode.

Available in: All
*/
void setFMAGCOverride(bool disableRFAGC, byte LNAGainIndex)
{
#define FM_AGC_OVERRIDE 0x28
#define FM_AGC_OVERRIDE_RFAGCDIS 0x01

    byte options = 0;
    if (disableRFAGC) options |= FM_AGC_OVERRIDE_RFAGCDIS;

    transmitCommand(
        "FM_AGC_OVERRIDE",
        FM_AGC_OVERRIDE, 0, true, false,
        options,
        LNAGainIndex);
}

/*
Enables output for GPO1, 2, and 3. GPO1, 2, and 3 can be configured for output (Hi-Z or active drive) by setting
the GPO1OEN, GPO2OEN, and GPO3OEN bit. The state (high or low) of GPO1, 2, and 3 is set with the
GPIO_SET command. To avoid excessive current consumption due to oscillation, GPO pins should not be left in a
high impedance state. The CTS bit (and optional interrupt) is set when it is safe to send the next command. This
command may only be sent when in powerup mode. The default is all GPO pins set for high impedance.

Notes:
1. GPIO_CTL is fully supported in FMRX component 2.0 or later. Only bit GPO3OEN is supported in FMRX component 1.0.
2. The use of GPO2 as an interrupt pin and/or the use of GPO3 as DCLK digital clock input will override this GPIO_CTL
function for GPO2 and/or GPO3 respectively.

Available in: All except Si4710-A10
*/
void setGPIOModes(bool enableGPIO1, bool enableGPIO2, bool enableGPIO3)
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

/*
Sets the output level (high or low) for GPO1, 2, and 3. GPO1, 2, and 3 can be configured for output by setting the
GPO1OEN, GPO2OEN, and GPO3OEN bit in the GPIO_CTL command. To avoid excessive current consumption
due to oscillation, GPO pins should not be left in a high impedance state. The CTS bit (and optional interrupt) is set
when it is safe to send the next command. This property may only be set or read when in powerup mode. The
default is all GPO pins set for high impedance.

Note: GPIO_SET is fully-supported in FMRX component 2.0 or later. Only bit GPO3LEVEL is supported in FMRX component
1.0.

Available in: All except Si4710-A10
*/
void setGPIOLevel(bool highGPIO1, bool highGPIO2, bool highGPIO3)
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

////////////////////////////////////////////////////////////
// CHIP API PROPERTIES
////////////////////////////////////////////////////////////

/*
Configures the sources for the GPO2/INT interrupt pin. Valid sources are the lower 8 bits of the STATUS byte,
including CTS, ERR, RSQINT, RDSINT (Si4705/21/31/35/37/39/41/43/45/85 only), and STCINT bits. The
corresponding bit is set before the interrupt occurs. The CTS bit (and optional interrupt) is set when it is safe to
send the next command. The CTS interrupt enable (CTSIEN) can be set with this property and the POWER_UP
command. The state of the CTSIEN bit set during the POWER_UP command can be read by reading this property
and modified by writing this property. This property may only be set or read when in powerup mode.

Errata:RSQIEN is non-functional on FMRX component 2.0.

Available in: All

Default: 0x0000
*/
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

/*
Configures the digital audio output format. Configuration options include DCLK edge, data format, force mono, and
sample precision.

Available in: Si4705/06, Si4721/31/35/37/39, Si4730/34/36/38-D60 and later, Si4741/43/45, Si4784/85

Default: 0x0000

Note: DIGITAL_OUTPUT_FORMAT is supported in FM receive component 2.0 or later.
*/
void setDigitalOutputFormat(bool dclkFallingEdge, uint16_t mode, bool forceMono, uint16_t samplePrecisionMode)
{
#define DIGITAL_OUTPUT_FORMAT 0x0102
#define DIGITAL_OUTPUT_FORMAT_OFALL_RISING 0x0000
#define DIGITAL_OUTPUT_FORMAT_OFALL_FALLING 0x0080
#define DIGITAL_OUTPUT_FORMAT_MODE_I2S 0x0000
#define DIGITAL_OUTPUT_FORMAT_MODE_LEFT_JUSTIFIED 0x0030
#define DIGITAL_OUTPUT_FORMAT_MODE_MSB_AT_2ND_DCLK 0x0040
#define DIGITAL_OUTPUT_FORMAT_MODE_MSB_AT_1ST_DCLK 0x0060
#define DIGITAL_OUTPUT_FORMAT_OMONO_USE_STEREO_BLEND 0x0000
#define DIGITAL_OUTPUT_FORMAT_OMONO_FORCE_MONO 0x0008
#define DIGITAL_OUTPUT_FORMAT_OSIZE_16 0x0000
#define DIGITAL_OUTPUT_FORMAT_OSIZE_20 0x0001
#define DIGITAL_OUTPUT_FORMAT_OSIZE_24 0x0002
#define DIGITAL_OUTPUT_FORMAT_OSIZE_8 0x0003

    uint16_t value = mode | samplePrecisionMode;
    if (dclkFallingEdge) value |= DIGITAL_OUTPUT_FORMAT_OFALL_FALLING;
    if (forceMono) value |= DIGITAL_OUTPUT_FORMAT_OMONO_FORCE_MONO;

    setProperty("DIGITAL_OUTPUT_FORMAT", DIGITAL_OUTPUT_FORMAT, value);
}

/*
Enables digital audio output and configures digital audio output sample rate in samples per second (sps). When
DOSR[15:0] is 0, digital audio output is disabled. The over-sampling rate must be set in order to satisfy a minimum
DCLK of 1 MHz. To enable digital audio output, program DOSR[15:0] with the sample rate in samples per second.
The system controller must establish DCLK and DFS prior to enabling the digital audio output else the
device will not respond and will require reset. The sample rate must be set to 0 before the DCLK/DFS is
removed. FM_TUNE_FREQ command must be sent after the POWER_UP command to start the internal
clocking before setting this property.

Note: DIGITAL_OUPTUT_SAMPLE_RATE is supported in FM receive component 2.0 or later.

Available in: Si4705/06, Si4721/31/35/37/39, Si4730/34/36/38-D60 and later, Si4741/43/45, Si4784/85

Default: 0x0000 (digital audio output disabled)

Units: sps

Range: 32–48 ksps, 0 to disable digital audio output
*/
void setDigitalAudioSampleRate(uint16_t value) 
{
#define DIGITAL_AUDIO_SAMPLE_RATE 0x0104

    setProperty("DIGITAL_AUDIO_SAMPLE_RATE", DIGITAL_AUDIO_SAMPLE_RATE, value);
}

/*
Sets the frequency of the REFCLK from the output of the prescaler. The REFCLK range is 31130 to 34406 Hz
(32768 ±5% Hz) in 1 Hz steps, or 0 (to disable AFC). For example, an RCLK of 13 MHz would require a prescaler
value of 400 to divide it to 32500 Hz REFCLK. The reference clock frequency property would then need to be set to
32500 Hz. RCLK frequencies between 31130 Hz and 40 MHz are supported, however, there are gaps in frequency
coverage for prescaler values ranging from 1 to 10, or frequencies up to 311300 Hz
*/
void setReferenceClockFrequency(uint16_t value)
{
#define REFCLK_FREQ 0x0201

    setProperty("REFCLK_FREQ", REFCLK_FREQ, value);
}

/*
Sets the number used by the prescaler to divide the external RCLK down to the internal REFCLK. The range may
be between 1 and 4095 in 1 unit steps. For example, an RCLK of 13 MHz would require a prescaler value of 400 to
divide it to 32500 Hz. The reference clock frequency property would then need to be set to 32500 Hz. The RCLK
must be valid 10 ns before sending and 20 ns after completing the AM_TUNE_FREQ and AM_SEEK_START
commands. In addition, the RCLK must be valid at all times for proper AFC operation. The RCLK may be removed
or reconfigured at other times. The CTS bit (and optional interrupt) is set when it is safe to send the next command.
This property may only be set or read when in powerup mode. The default is 1.

Available in: All

Default: 0x0001

Step: 1

Range: 1–4095
*/
void setReferenceClockPrescale(uint16_t value, bool useDCLK = false)
{
#define REFCLK_PRESCALE 0x0202
#define REFCLK_PRESCALE_RCLKSEL_RCLK 0x0000
#define REFCLK_PRESCALE_RCLKSEL_DCLK 0x1000

    if (useDCLK) value |= REFCLK_PRESCALE_RCLKSEL_DCLK;

    setProperty("REFCLK_PRESCALE", REFCLK_FREQ, value);
}


/*
Sets the audio output volume. The CTS bit (and optional interrupt) is set when it is safe to send the next command.
This property may only be set or read when in powerup mode. The default is 63.

Available in: All except Si4749

Default: 0x003F

Step: 1

Range: 0–63
*/
void setVolume(byte value) 
{
#define RX_VOLUME 0x4000

    setProperty("RX_VOLUME", RX_VOLUME, value);
}

/*
Mutes the audio output. L and R audio outputs may be muted independently. The CTS bit (and optional interrupt) is
set when it is safe to send the next command. This property may only be set or read when in powerup mode. The
default is unmute (0x0000).

Available in: All except Si4749

Default: 0x0000
*/
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

////////////////////////////////////////////////////////////
// FINITE STATE MACHINE
////////////////////////////////////////////////////////////

void step_powerUp()
{
    powerUp(POWER_UP_FUNC_FM_RCV);
    nextStep = step_enableGPIO;
}

// enable GPIO pins, enable all to avoid excessive current consumption
// due to oscillation.
void step_enableGPIO()
{
    setGPIOModes(true, true, true);
    nextStep = step_setInterruptSource;
}

void step_setInterruptSource()
{
    setGPIOInterruptSources(false, false, false, true, true, true, false, true);
    nextStep = step_setReferenceClockPrescale;
}

void step_setReferenceClockPrescale()
{
    setReferenceClockPrescale(1);
    nextStep = step_setReferenceClockFrequency;
}

void step_setReferenceClockFrequency()
{
    setReferenceClockFrequency(31250);
    nextStep = step_getCurrentTuning1;
}

void step_getCurrentTuning1()
{
    getFMTuneStatus();
    nextStep = step_printStation1;
}

void step_printStation1()
{
    printStation();
    nextStep = step_tuneToStation1;
}

void step_printStation2()
{
    printStation();
    nextStep = step_seekToStation;
}

void step_tuneToStation1()
{
    setFMTuneFrequency(8850);
    nextStep = step_getInterruptStatus1;
}

void step_getInterruptStatus1()
{
    getInterruptStatus();
    nextStep = step_getTuneStatus1;
}

void step_getInterruptStatus2()
{
    getInterruptStatus();
    nextStep = step_getTuneStatus1;
}

void step_getTuneStatus1()
{
    getFMTuneStatus();
    nextStep = step_printStation2;
}

void step_seekToStation()
{
    seekFM(true, true);
    nextStep = step_getInterruptStatus2;
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
}

#ifdef USE_SOFTWARE_CLOCK
ISR(TIMER1_COMPA_vect)
{
    digitalWrite(CLOCK_PIN, clockPinState = !clockPinState);
}
#endif

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

    nextStep = step_powerUp;
}