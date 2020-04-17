@echo off

rem currently, GCC 8.3.0 has been used, see
rem http://gnutoolchains.com/raspberry/
rem *** edit this to point to your Raspi GCC.
set "compiler_path=C:\SysGCC\Raspberry\bin"

set "fw_ver=V1.40"

set "PATH=%compiler_path%;%PATH%"
set "compiler=%compiler_path%\arm-linux-gnueabihf-gcc.exe"

rem get the current directory
set "starting_dir=%CD%"

rem changes the current directory to the directory where this batch file is.
rem not necessary when starting via the Windows explorer, but from IDEs or so.
cd "%~dp0"

echo Generating CT800 32 bit for Raspberry Pi.
<nul set /p dummy_variable="GCC version: "
"%compiler%" -dumpversion
rem *** the source files are fetched relative to the path of this batch file
set "compiler_options=-DTARGET_BUILD=32 -pthread -Wall -Wextra -Wlogical-op -Wstrict-prototypes -Werror -O02 -flto -std=c99 -faggressive-loop-optimizations -fno-unsafe-loop-optimizations -fgcse-sm -fgcse-las -fgcse-after-reload -fno-strict-aliasing -fno-strict-overflow -lrt -Wl,-s"
"%compiler%" %compiler_options% -o output\CT800_%fw_ver%_rasp_x32 play.c kpk.c eval.c move_gen.c hashtables.c search.c util.c book.c

rem go back to the starting directory
cd "%starting_dir%"

pause