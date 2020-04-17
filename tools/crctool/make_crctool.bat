@echo off

rem currently, MingW GCC 7.3.0 has been used.
rem *** edit this to point to your MingW GCC.
set "compiler_path=C:\mingw-w64\i686-7.3.0-posix-dwarf-rt_v5-rev0\mingw32\bin"

set "PATH=%compiler_path%;%PATH%"
set "compiler=%compiler_path%\gcc.exe"

rem get the current directory
set "starting_dir=%CD%"

rem changes the current directory to the directory where this batch file is.
rem not necessary when starting via the Windows explorer, but from IDEs or so.
cd "%~dp0"

echo.
echo Generating CRC tool...
<nul set /p dummy_variable="GCC version: "
"%compiler%" -dumpversion
rem *** the source files are fetched relative to the path of this batch file
set "compiler_options=-Wall -Wextra -Wlogical-op -Wstrict-prototypes -Werror -O2 -s -std=c99 -fno-strict-aliasing -fno-strict-overflow -fno-set-stack-executable -mconsole -static -pie -fPIE -Wl,-e,_mainCRTStartup -Wl,--dynamicbase -Wl,--nxcompat -Wl,-s"
"%compiler%" %compiler_options% ./crctool.c -o ./crctool_win.exe

rem go back to the starting directory
cd "%starting_dir%"

pause