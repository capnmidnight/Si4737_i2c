// HAL.h

#ifndef _HAL_h
#define _HAL_h

#include <arduino.h>

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

#define POWER_UP_FUNC_FM_RCV 0x00
#define POWER_UP_FUNC_WB_RCV 0x03
#define POWER_UP_FUNC_QUERY_LIB_ID 0x0F

void printBuffer(const char* name, const size_t size, const byte* buffer, int radix = 16);
void prepareChip();
void sleep(int millis);
void wait(int seconds);
void printStation();

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
GPO2/INT is driven high during normal operation and low for a minimum of 1 �s during the interrupt. The CTSIEN
bit is duplicated in the GPO_IEN property. The command is complete when the CTS bit (and optional interrupt) is
set.

Note: To change function (e.g. FM RX to AM RX or FM RX to FM TX), issue POWER_DOWN command to stop current function;
then, issue POWER_UP to start new function.

Note: Delay at least 500 ms between powerup command and first tune command to wait for the oscillator to stabilize if
XOSCEN is set and crystal is used as the RCLK.

Available in: All

Response Bytes: None (FUNC != QUERY_LIBRARY_ID), Seven (FUNC = QUERY_LIBRARY_ID)
*/
void powerUp(byte funcMode, bool analogOutput = true, bool digitalOutput = false, bool enableCTSInterrupt = true, bool enableGP02 = true, bool enableXOSC = false, bool enablePatch = false);

/*
Returns the part number, chip revision, firmware revision, patch revision and component revision numbers.The
command is complete when the CTS bit(and optional interrupt) is set.This command may only be sent when in
powerup mode.

Available in : All

Response bytes : Fifteen(Si4705 / 06 only), Eight(Si4704 / 2x / 3x / 4x)
*/
void printRevision(bool isSI4705or06 = false);

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
void powerDown();

/*
Sets a property shown in Table 9, �FM/RDS Receiver Property Summary,� on page 56. The CTS bit (and optional
interrupt) is set when it is safe to send the next command. This command may only be sent when in powerup
mode. See Figure 29, �CTS and SET_PROPERTY Command Complete tCOMP Timing Model,� on page 226 and
Table 46, �Command Timing Parameters for the FM Receiver,� on page 228.

Available in: All
*/
void setProperty(const char* name, uint16_t propertyNumber, uint16_t propertyValue);

/*
Gets a property as shown in AN332: Table 9, �FM/RDS Receiver Property Summary,� on page 56. The CTS bit (and
optional interrupt) is set when it is safe to send the next command. This command may only be sent when in
powerup mode.

Available in: All

Response bytes: Three
*/
uint16_t getProperty(const char* name, uint16_t propertyNumber);

/*
Updates bits 6:0 of the status byte. This command should be called after any command that sets the STCINT,
RDSINT, or RSQINT bits. When polling this command should be periodically called to monitor the STATUS byte,
and when using interrupts, this command should be called after the interrupt is set to update the STATUS byte. The
CTS bit (and optional interrupt) is set when it is safe to send the next command. This command may only be set
when in powerup mode.

Available in: All
*/
bool getInterruptStatus();

void waitForSTC();

/*
Sets the FM Receive to tune a frequency between 64 and 108 MHz in 10 kHz units. The CTS bit (and optional
interrupt) is set when it is safe to send the next command. The ERR bit (and optional interrupt) is set if an invalid
argument is sent. Note that only a single interrupt occurs if both the CTS and ERR bits are set. The optional STC
interrupt is set when the command completes. The STCINT bit is set only after the GET_INT_STATUS command is
called. This command may only be sent when in powerup mode. The command clears the STC bit if it is already
set. See Figure 28, �CTS and STC Timing Model,� on page 226 and Table 46, �Command Timing Parameters for
the FM Receiver,� on page 228.

FM: LO frequency is 128 kHz above RF for RF frequencies < 90 MHz and 128 kHz below RF for RF frequencies >
90 MHz. For example, LO frequency is 80.128 MHz when tuning to 80.00 MHz.

Note: For FMRX components 2.0 or earlier, tuning range is 76�108 MHz.

Note: Fast bit is supported in FMRX components 4.0 or later.

Note: Freeze bit is supported in FMRX components 4.0 or later.

Available in: All
*/
void setFMTuneFrequency(int f, bool freezeMetrics = false, bool fastTune = false, byte antennaCapValue = 0);

/*
Begins searching for a valid frequency. Clears any pending STCINT or RSQINT interrupt status. The CTS bit (and
optional interrupt) is set when it is safe to send the next command. RSQINT status is only cleared by the RSQ
status command when the INTACK bit is set. The ERR bit (and optional interrupt) is set if an invalid argument is
sent. Note that only a single interrupt occurs if both the CTS and ERR bits are set. The optional STC interrupt is set
when the command completes. The STCINT bit is set only after the GET_INT_STATUS command is called. This
command may only be sent when in powerup mode. The command clears the STCINT bit if it is already set. See
Figure 28, �CTS and STC Timing Model,� on page 226 and Table 46, �Command Timing Parameters for the FM
Receiver,� on page 228.

Available in: All
*/
void seekFM(bool seekUp, bool wrap);

/*
Returns the status of FM_TUNE_FREQ or FM_SEEK_START commands. The command returns the current
frequency, RSSI, SNR, multipath, and the antenna tuning capacitance value (0-191). The command clears the
STCINT interrupt bit when INTACK bit of ARG1 is set. The CTS bit (and optional interrupt) is set when it is safe to
send the next command. This command may only be sent when in powerup mode.

Available in: All

Response bytes: Seven
*/
byte getFMTuneStatus(bool cancelSeek = false, bool ackSTC = true);

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
void setGPIOModes(bool enableGPIO1, bool enableGPIO2, bool enableGPIO3);

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
void setGPIOLevel(bool highGPIO1, bool highGPIO2, bool highGPIO3);

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
void setGPIOInterruptSources(bool repeatRSQ, bool repeatRDS, bool repeatSTC, bool enableCTS, bool enableERR, bool enableRSQ, bool enableRDS, bool enableSTC);

/*
Sets the frequency of the REFCLK from the output of the prescaler. The REFCLK range is 31130 to 34406 Hz
(32768 �5% Hz) in 1 Hz steps, or 0 (to disable AFC). For example, an RCLK of 13 MHz would require a prescaler
value of 400 to divide it to 32500 Hz REFCLK. The reference clock frequency property would then need to be set to
32500 Hz. RCLK frequencies between 31130 Hz and 40 MHz are supported, however, there are gaps in frequency
coverage for prescaler values ranging from 1 to 10, or frequencies up to 311300 Hz
*/
void setReferenceClockFrequency(uint16_t value);
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

Range: 1�4095
*/
void setReferenceClockPrescale(uint16_t value, bool useDCLK = false);


/*
Sets the audio output volume. The CTS bit (and optional interrupt) is set when it is safe to send the next command.
This property may only be set or read when in powerup mode. The default is 63.

Available in: All except Si4749

Default: 0x003F

Step: 1

Range: 0�63
*/
void setVolume(byte value);

/*
Mutes the audio output. L and R audio outputs may be muted independently. The CTS bit (and optional interrupt) is
set when it is safe to send the next command. This property may only be set or read when in powerup mode. The
default is unmute (0x0000).

Available in: All except Si4749

Default: 0x0000
*/
void setHardMute(bool muteLeft, bool muteRight);

#endif

