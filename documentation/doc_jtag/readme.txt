If you are running Windows, you can use the Coocox standalone programming
software "CoFlash" for flashing the firmware to the controller. The two
screenshots in this directory show the necessary settings.

Since the Coocox website seems to be down sometimes, I uploaded CoFlash
in case you do not already have it:

https://www.ct800.net/download/coflash.zip

The used JTAG adaptor is ARM-JTAG-Coocox, see here for Windows drivers
(which you need additionally to CoFlash) and installation instructions:

https://www.olimex.com/Products/ARM/JTAG/ARM-JTAG-COOCOX/

Note: It may happen that Windows does not find the programming adaptor even
after installing the drivers. With my mainboard, the solution was to go into
the BIOS and configure the USB ports to "Full Speed", which means USB-2 in
slow configuration at 12 MBit/s. The "High Speed" setting at 480 MBit/s did
not work. According to Olimex, this is a very rare problem.

Note: When using CoFlash, it may happen that it says:
"USB communication failed."
In that case, exit CoFlash, pull the USB cable out of the programming
adaptor, start CoFlash again, put the USB cable into the programming
adaptor and try to flash again. Usually, this will work. If not, try
closing all other applications and flash again.