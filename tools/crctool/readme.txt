This tool appends a CRC32 to the BIN file and converting it to HEX file
format (Intel Hex32).

There are 16 data bytes per line in the HEX file (not configurable). Up
to 255 bytes would be legal, but quite some device programming
applications do not work with more than 16. Therefore, choosing 16 data
bytes per line ensures maximum compatibility.

For Linux:

- run make_crctool.sh
- copy the BIN file into this directory: ct800fw.bin
(must be <=384k - else, change the crc tool source text accordingly)
- run run_crctool.sh
- use ct800fw_crc.hex / ct800fw_crc.bin for firmware flashing.


For Windows:

use the batch file crctool_win.bat, assuming that the firmware name
is "ct800fw.bin". Or use crctool_win.exe from your IDE as post-linkage
step, giving the path of the firmware file and the ROM start address
in hex.

note: the precompiled Windows binary is for convenience only. Compiled
with MS CL V10.0 using the /O2 /MT switches.


note: the BIN format does not contain the starting address of the flash.
This is given as a paramter to the tool, or hardcoded for a default.
So the HEX file is complete.

note: the conversion to the hexfile format has been verified using:
https://sourceforge.net/projects/hex2bin/
when using hex2bin on the hexfile ct800fw_crc.hex , the result exactly
matches the binary file ct800fw_crc.bin .