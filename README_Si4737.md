Arduino Si4737 I2C Library, adapted from [Radu - Eosif Mihailescu](https://github.com/csdexter/Si4735)
See original source for more details.

This code is for the Si4737 breakout board made by [Alex_K](http://forum.arduino.cc/index.php?topic=145187.msg1420076#msg1420076) of Arudino forums

I could not get the Si4735 library to work with my board, so I figured I'd start from blank. Anything and everything concerning SPI has been removed. An example is available. 

=== Hardware Notes ===
 * Quick connection reference:
   I2C Mode, Si4735 -> Arduino:
     SDIO      -> SDA     (Arduino bidirectional)
     SCLK      -> SCL     (Arduino output)
     #RESET    -> D9      (Arduino output)
	 
	 I don't know what those are:
	 #SEN      -> tied high or low, tell the code which one with
                  SI4735_PIN_SEN_HW*
     GPO1      -> left floating
     GPO2/#INT -> D2/INT0 (Arduino input)
                  or (left floating or tied high, if not using interrupts)

