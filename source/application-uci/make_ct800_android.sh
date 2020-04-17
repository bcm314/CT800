#!/bin/bash
# currently, Android NDK r21 (64 bit Windows version) has been used.
# replace the "windows-x86_64" with whatever host system you are using:
# Windows 32 bit: windows
# Windows 64 bit: windows-x86_64
# Linux 64 bit:   linux-x86_64
# macOS 64 bit:   darwin-x86_64
# *** edit this to point to your Android Clang.
compiler_path="C:/android-ndk-r21/toolchains/llvm/prebuilt/windows-x86_64/bin"
fw_ver="V1.40"
# *** derived variables - do not edit
compiler="$compiler_path/clang"
# *** target options
arm64opt="-target aarch64-linux-android21"
arm32opt="-target armv7a-linux-androideabi16"
x86_64opt="-target x86_64-linux-android21"
x86_32opt="-target i686-linux-android16"
# save current directory
starting_dir=$(pwd)
# change the path to where this script is - useful in case this script
# is being called from somewhere else, like from within an IDE
cd "$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
# *** the source files are fetched relative to the path of this script
echo "Generating CT800 64 bit for ARM-Android."
compiler_options="-DTARGET_BUILD=64 -pie -fPIE -Wl,-pie -Wall -Wextra -Wstrict-prototypes -Werror -O2 -std=c99 -fno-strict-aliasing -fno-strict-overflow -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,-s"
"$compiler" $arm64opt $compiler_options -o output/CT800_${fw_ver}_andarm64 play.c kpk.c eval.c move_gen.c hashtables.c search.c util.c book.c
echo "Generating CT800 32 bit for ARM-Android."
# -DNO_MONO_COND because Android NDK before API level 21 does not support monotonic clocks in pthread conditions.
compiler_options="-DTARGET_BUILD=32 -DNO_MONO_COND -pie -fPIE -Wl,-pie -mthumb -Wl,--fix-cortex-a8 -Wall -Wextra -Wstrict-prototypes -Werror -O2 -std=c99 -fno-strict-aliasing -fno-strict-overflow -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,-s"
"$compiler" $arm32opt $compiler_options -o output/CT800_${fw_ver}_andarm32 play.c kpk.c eval.c move_gen.c hashtables.c search.c util.c book.c
echo "Generating CT800 64 bit for x86-Android."
compiler_options="-DTARGET_BUILD=64 -pie -fPIE -Wl,-pie -Wall -Wextra -Wstrict-prototypes -Werror -O2 -std=c99 -fno-strict-aliasing -fno-strict-overflow -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,-s"
"$compiler" $x86_64opt $compiler_options -o output/CT800_${fw_ver}_andx86_64 play.c kpk.c eval.c move_gen.c hashtables.c search.c util.c book.c
echo "Generating CT800 32 bit for x86-Android."
# -DNO_MONO_COND because Android NDK before API level 21 does not support monotonic clocks in pthread conditions.
compiler_options="-DTARGET_BUILD=32 -DNO_MONO_COND -pie -fPIE -Wl,-pie -Wall -Wextra -Wstrict-prototypes -Werror -O2 -std=c99 -fno-strict-aliasing -fno-strict-overflow -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,-s"
"$compiler" $x86_32opt $compiler_options -o output/CT800_${fw_ver}_andx86_32 play.c kpk.c eval.c move_gen.c hashtables.c search.c util.c book.c
# go back to the starting directory
cd "$starting_dir"
read -n1 -r -p "press any key to continue..." key
