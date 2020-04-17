@echo off

rem currently, GCC-ARM 7.3.1 NONE-EABI has been used.
rem *** edit this to point to your ARM GCC.
set "compiler_path=C:\gcc-arm-none-eabi-7_3_Q2\"

set "fw_ver=V1.40"

rem *** derived variable - do not edit
set "compiler=%compiler_path%bin\arm-none-eabi-gcc.exe"
set "obj_cpy=%compiler_path%bin\arm-none-eabi-objcopy.exe"
set "obj_size=%compiler_path%bin\arm-none-eabi-size.exe"

rem get the current directory
set "starting_dir=%CD%"

rem changes the current directory to the directory where this batch file is.
rem not necessary when starting via the Windows explorer, but from IDEs or so.
cd "%~dp0"

rem if the compiler path is misconfigured, give a useful error message.
if not exist "%compiler%" (
    rem check whether ARM GCC is installed in the PATH.
    arm-none-eabi-gcc.exe -dumpversion >nul 2>&1 && (
        rem if the special path at the top of this shell script does
        rem not work, but there is a system wide ARM-GCC, take that one.
        set "compiler=arm-none-eabi-gcc.exe"
        set "obj_cpy=arm-none-eabi-objcopy.exe"
    ) || (
        echo ****************************
        echo ARM GCC not installed under:
        echo %compiler_path%
        echo edit this batch file.
        echo build failed.
        echo ****************************
        echo.
        goto :END_OF_BUILD)
    )
)

rem check presence of tools
if not exist "tool_bin\booktool_win.exe" (
    echo *************************************
    echo booktool_win.exe missing in tool_bin.
    echo build failed.
    echo *************************************
    echo.
    goto :END_OF_BUILD)
if not exist "tool_bin\crctool_win.exe" (
    echo ************************************
    echo crctool_win.exe missing in tool_bin.
    echo build failed.
    echo ************************************
    echo.
    goto :END_OF_BUILD)

rem create the build directories if necessary
if not exist CT800 (mkdir CT800)
if not exist CT800\Debug (mkdir CT800\Debug)
if not exist CT800\Debug\bin (mkdir CT800\Debug\bin)
if not exist CT800\Debug\obj (mkdir CT800\Debug\obj)

del CT800FW_CRC_%fw_ver%.hex >nul 2>&1
del CT800FW_CRC_%fw_ver%.bin >nul 2>&1
del CT800FW_%fw_ver%.map >nul 2>&1

echo **********************
echo making opening book...
echo **********************
cd tool_bin
copy ..\..\tools\booktool\bookdata.txt . >nul 2>&1
if not exist "bookdata.txt" (
    echo.
    echo *********************
    echo opening book missing.
    echo build failed.
    echo *********************
    echo.
    goto :END_OF_BUILD)
booktool_win bookdata.txt
echo.
del bookdata.txt >nul 2>&1
if not exist "bookdata.c" (
    echo ********************
    echo could not make book.
    echo build failed.
    echo ********************
    echo.
    goto :END_OF_BUILD)
cd ..
move tool_bin\bookdata.c application\ >nul 2>&1

cd CT800\Debug\obj
del *.o >nul 2>&1
del *.xml >nul 2>&1

echo ************
echo compiling...
echo ************
echo.
<nul set /p dummy_variable="ARM GCC version: "
"%compiler%" -dumpversion
rem *** the source files are fetched relative to the path of this batch file
set "compiler_common_options=-fstack-usage -Wall -Wextra -Wlogical-op -Wstrict-prototypes -Werror -O2 -std=c99 -mcpu=cortex-m4 -mtune=cortex-m4 -mthumb -mfloat-abi=soft -fomit-frame-pointer -ffreestanding -ffunction-sections -mslow-flash-data -fno-strict-aliasing -fno-strict-overflow -DSTM32F405RG -DSTM32F405 -D__ASSEMBLY__ -I..\..\..\application"
"%compiler%" %compiler_common_options% -c ..\..\..\application\boot_stm32f405.c ..\..\..\application\arm_driver.c ..\..\..\application\hmi.c ..\..\..\application\menu.c ..\..\..\application\posedit.c ..\..\..\application\play.c ..\..\..\application\timekeeping.c ..\..\..\application\hardware_arm.c ..\..\..\application\hardware_arm_disp.c ..\..\..\application\hardware_arm_keybd.c ..\..\..\application\hardware_arm_signal.c ..\..\..\application\kpk.c ..\..\..\application\util.c ..\..\..\application\book.c ..\..\..\application\hashtables.c ..\..\..\application\eval.c ..\..\..\application\move_gen.c ..\..\..\application\search.c

cd ..
cd bin
del *.bin >nul 2>&1
del *.elf >nul 2>&1
del *.hex >nul 2>&1
del *.map >nul 2>&1

echo.
echo **********
echo linking...
echo **********
echo.
"%compiler%" -O2 -mcpu=cortex-m4 -mtune=cortex-m4 -mthumb -nostartfiles -nodefaultlibs -nostdlib -ffreestanding -Wl,-Map=ct800fw.map -Wl,--entry=main -Wl,--gc-sections -Wl,-T..\..\..\arm-gcc-link.ld -o ct800fw.elf ..\obj\boot_stm32f405.o ..\obj\arm_driver.o ..\obj\hardware_arm.o ..\obj\hardware_arm_disp.o ..\obj\hardware_arm_keybd.o ..\obj\hardware_arm_signal.o ..\obj\util.o ..\obj\hmi.o ..\obj\timekeeping.o ..\obj\play.o ..\obj\menu.o ..\obj\posedit.o ..\obj\move_gen.o ..\obj\kpk.o ..\obj\book.o ..\obj\hashtables.o ..\obj\eval.o ..\obj\search.o

rem if the linkage has failed, abort here.
if not exist "ct800fw.elf" (
    echo.
    echo **************************
    echo could not link executable.
    echo build failed.
    echo **************************
    echo.
    goto :END_OF_BUILD)

echo executable info:
"%obj_size%" ct800fw.elf
echo.

echo ****************
echo appending crc...
echo ****************

"%obj_cpy%" -O binary ct800fw.elf ct800fw.bin
..\..\..\tool_bin\crctool_win.exe ct800fw.bin 0x08000000

copy ct800fw_crc.hex ..\..\..\CT800FW_CRC_%fw_ver%.hex >nul 2>&1
copy ct800fw_crc.bin ..\..\..\CT800FW_CRC_%fw_ver%.bin >nul 2>&1
copy ct800fw.map ..\..\..\CT800FW_%fw_ver%.map >nul 2>&1

echo.
echo *************************
echo OK: CT800FW_CRC_%fw_ver%.bin
echo OK: CT800FW_CRC_%fw_ver%.hex
echo done.
echo *************************
echo.

:END_OF_BUILD

rem go back to the starting directory
cd "%starting_dir%"

pause