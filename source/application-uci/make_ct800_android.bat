@echo off
rem currently, Android NDK r21 (64 bit Windows version) has been used.
rem replace the "windows-x86_64" with whatever host system you are using:
rem Windows 32 bit: windows
rem Windows 64 bit: windows-x86_64
rem Linux 64 bit:   linux-x86_64
rem macOS 64 bit:   darwin-x86_64
set "compiler_path=C:\android-ndk-r21\toolchains\llvm\prebuilt\windows-x86_64\bin"

set "fw_ver=V1.40"
set "compiler=%compiler_path%\clang"

rem target options
set "arm64opt=-target aarch64-linux-android21"
set "arm32opt=-target armv7a-linux-androideabi16"
set "x86_64opt=-target x86_64-linux-android21"
set "x86_32opt=-target i686-linux-android16"

rem get the current directory
set "starting_dir=%CD%"

rem changes the current directory to the directory where this batch file is.
rem not necessary when starting via the Windows explorer, but from IDEs or so.
cd "%~dp0"

rem it is not necessary to add the compiler paths to PATH, but it looks more
rem future proof to have that.
set "OLD_PATH=%PATH%"
set "PATH=%compiler_path%;%OLD_PATH%"

rem *** the source files are fetched relative to the path of this batch file

echo Generating CT800 64 bit for ARM-Android.
set "compiler_options=-DTARGET_BUILD=64 -pie -fPIE -Wl,-pie -Wall -Wextra -Wstrict-prototypes -Werror -O2 -std=c99 -fno-strict-aliasing -fno-strict-overflow -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,-s"
cmd /c "%compiler%" %arm64opt% %compiler_options% -o output\CT800_%fw_ver%_andarm64 play.c kpk.c eval.c move_gen.c hashtables.c search.c util.c book.c

echo Generating CT800 32 bit for ARM-Android.
rem -DNO_MONO_COND because Android NDK before API level 21 does not support monotonic clocks in pthread conditions.
set "compiler_options=-DTARGET_BUILD=32 -DNO_MONO_COND -pie -fPIE -Wl,-pie -mthumb -Wall -Wextra -Wstrict-prototypes -Werror -O2 -std=c99 -fno-strict-aliasing -fno-strict-overflow -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,-s"
cmd /c "%compiler%" %arm32opt% %compiler_options% -o output\CT800_%fw_ver%_andarm32 play.c kpk.c eval.c move_gen.c hashtables.c search.c util.c book.c

echo Generating CT800 64 bit for x86-Android.
set "compiler_options=-DTARGET_BUILD=64 -pie -fPIE -Wl,-pie -Wall -Wextra -Wstrict-prototypes -Werror -O2 -std=c99 -fno-strict-aliasing -fno-strict-overflow -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,-s"
cmd /c "%compiler%" %x86_64opt% %compiler_options% -o output\CT800_%fw_ver%_andx86_64 play.c kpk.c eval.c move_gen.c hashtables.c search.c util.c book.c

echo Generating CT800 32 bit for x86-Android.
rem -DNO_MONO_COND because Android NDK before API level 21 does not support monotonic clocks in pthread conditions.
set "compiler_options=-DTARGET_BUILD=32 -DNO_MONO_COND -pie -Wl,-pie -fPIE -Wall -Wextra -Wstrict-prototypes -Werror -O2 -std=c99 -fno-strict-aliasing -fno-strict-overflow -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,-s"
cmd /c "%compiler%" %x86_32opt% %compiler_options% -o output\CT800_%fw_ver%_andx86_32 play.c kpk.c eval.c move_gen.c hashtables.c search.c util.c book.c

set "PATH=%OLD_PATH%"

rem go back to the starting directory
cd "%starting_dir%"

pause
