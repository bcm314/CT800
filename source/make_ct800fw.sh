#!/bin/bash
# Currently, GCC-ARM 7.3.1 NONE-EABI has been used.
# *** edit this to point to your ARM GCC.
compiler_path="C:/gcc-arm-none-eabi-7_3_Q2/"
fw_ver="V1.40"
# *** derived variables - do not edit
compiler_sub="bin/arm-none-eabi-gcc"
obj_cpy_sub="bin/arm-none-eabi-objcopy"
obj_size_sub="bin/arm-none-eabi-size"
compiler="$compiler_path$compiler_sub"
obj_cpy="$compiler_path$obj_cpy_sub"
obj_size="$compiler_path$obj_size_sub"
# if the compiler path is misconfigured, give a useful error message.
echo ""
if [ ! -x "$compiler" ]; then
    # check whether ARM GCC is installed in the PATH.
    if [ ! -x "$(command -v arm-none-eabi-gcc)" ]; then
        echo "*************************************"
        echo "ARM GCC (target) not installed under:"
        echo $compiler_path
        echo "edit this shell script.              "
        echo "build failed.                        "
        echo "*************************************"
        echo ""
        read -n1 -r -p "press any key to continue..." key
        exit 1
    fi
    # if the special path at the top of this shell script does
    # not work, but there is a system wide ARM-GCC, take that one.
    compiler="arm-none-eabi-gcc"
    obj_cpy="arm-none-eabi-objcopy"
fi
# save current directory
starting_dir=$(pwd)
# change the path to where this script is - useful in case this script
# is being called from somewhere else, like from within an IDE
cd "$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
# create the build directories if necessary
if [ ! -d "./CT800" ]; then
    mkdir ./CT800 >&/dev/null
fi
if [ ! -d "./CT800/Debug" ]; then
    mkdir ./CT800/Debug >&/dev/null
fi
if [ ! -d "./CT800/Debug/bin" ]; then
    mkdir ./CT800/Debug/bin >&/dev/null
fi
if [ ! -d "./CT800/Debug/obj" ]; then
    mkdir ./CT800/Debug/obj >&/dev/null
fi
# delete old output files
rm ./CT800FW_CRC_${fw_ver}.hex >&/dev/null
rm ./CT800FW_CRC_${fw_ver}.bin >&/dev/null
rm ./CT800FW_${fw_ver}.map >&/dev/null
# if the CRC tool has not yet been prepared, do so
if [ ! -x "./tool_bin/crctool" ]; then
    cd ../tools/crctool/
    echo "******************"
    echo "making CRC tool..."
    echo "******************"
    echo ""
    # check whether host GCC is installed
    if  [ ! -x "$(command -v gcc)" ]; then
        echo "*************************"
        echo "GCC (host) not installed."
        echo "build failed.            "
        echo "*************************"
        echo ""
        # go back to the starting directory
        cd "$starting_dir"
        read -n1 -r -p "press any key to continue..." key
        exit 1
    fi
    #does the make script have execution rights?
    if [ -f "./make_crctool.sh" ]; then
        if [ ! -x "./make_crctool.sh" ]; then
            chmod +x ./make_crctool.sh
        fi
    else
        # no make CRC tool script?!
        echo "*****************************"
        echo "CRC tool make script missing."
        echo "build failed.                "
        echo "*****************************"
        echo ""
        # go back to the starting directory
        cd "$starting_dir"
        read -n1 -r -p "press any key to continue..." key
        exit 1
    fi
    rm ./crctool >&/dev/null
	./make_crctool.sh
	cp ./crctool ../../source/tool_bin/ >&/dev/null
	cd ../../source/
fi
# if the CRC tool is still missing, something is wrong
if [ ! -x "./tool_bin/crctool" ]; then
    echo ""
    echo "************************"
    echo "could not make CRC tool."
    echo "build failed.           "
    echo "************************"
    echo ""
	# go back to the starting directory
    cd "$starting_dir"
    read -n1 -r -p "press any key to continue..." key
    exit 1
fi
# if the book tool has not yet been prepared, do so
if [ ! -x "./tool_bin/booktool" ]; then
    cd ../tools/booktool/
    echo "*******************"
    echo "making book tool..."
    echo "*******************"
    echo ""
    # check whether host GCC is installed
    if  [ ! -x "$(command -v gcc)" ]; then
        echo "*************************"
        echo "GCC (host) not installed."
        echo "build failed.            "
        echo "*************************"
        echo ""
        # go back to the starting directory
        cd "$starting_dir"
        read -n1 -r -p "press any key to continue..." key
        exit 1
    fi
    #does the make script have execution rights?
    if [ -f "./make_booktool.sh" ]; then
        if [ ! -x "./make_booktool.sh" ]; then
            chmod +x ./make_booktool.sh
        fi
    else
        # no make book tool script?!
        echo "******************************"
        echo "book tool make script missing."
        echo "build failed.                 "
        echo "******************************"
        echo ""
        # go back to the starting directory
        cd "$starting_dir"
        read -n1 -r -p "press any key to continue..." key
        exit 1
    fi
    rm ./booktool >&/dev/null
	./make_booktool.sh
	cp ./booktool ../../source/tool_bin/ >&/dev/null
	cd ../../source/
fi
# if the book tool is still missing, something is wrong
if [ ! -x "./tool_bin/booktool" ]; then
    echo ""
    echo "*************************"
    echo "could not make book tool."
    echo "build failed.            "
    echo "*************************"
    echo ""
	# go back to the starting directory
    cd "$starting_dir"
    read -n1 -r -p "press any key to continue..." key
    exit 1
fi
echo "**********************"
echo "making opening book..."
echo "**********************"
cd ./tool_bin
cp ../../tools/booktool/bookdata.txt . >&/dev/null
# check if the opening book is missing
if [ ! -f "./bookdata.txt" ]; then
    echo ""
    echo "*********************"
    echo "opening book missing."
    echo "build failed.        "
    echo "*********************"
    echo ""
	# go back to the starting directory
    cd "$starting_dir"
    read -n1 -r -p "press any key to continue..." key
    exit 1
fi
./booktool bookdata.txt
rm ./bookdata.txt >&/dev/null
if [ ! -f "./bookdata.c" ]; then
    echo ""
    echo "********************"
    echo "could not make book."
    echo "build failed.       "
    echo "********************"
    echo ""
    # go back to the starting directory
    cd "$starting_dir"
    read -n1 -r -p "press any key to continue..." key
    exit 1
fi
cd ..
mv ./tool_bin/bookdata.c ./application/
cd ./CT800/Debug/obj/
rm ./*.o >&/dev/null
rm ./*.xml >&/dev/null
echo ""
echo "************"
echo "compiling..."
echo "************"
echo ""
echo ARM GCC version: $("$compiler" -dumpversion)
# *** the source files are fetched relative to the path of this script
compiler_common_options="-Wall -Wextra -Wlogical-op -Wstrict-prototypes -Werror -O2 -std=c99 -mcpu=cortex-m4 -mtune=cortex-m4 -mthumb -mfloat-abi=soft -fomit-frame-pointer -ffreestanding -ffunction-sections -mslow-flash-data -fno-strict-aliasing -fno-strict-overflow -DSTM32F405RG -DSTM32F405 -D__ASSEMBLY__ -I../../../application"
"$compiler" $compiler_common_options -c ../../../application/boot_stm32f405.c ../../../application/arm_driver.c ../../../application/hmi.c ../../../application/menu.c ../../../application/posedit.c ../../../application/play.c ../../../application/timekeeping.c ../../../application/hardware_arm.c ../../../application/hardware_arm_disp.c ../../../application/hardware_arm_keybd.c ../../../application/hardware_arm_signal.c ../../../application/kpk.c ../../../application/util.c ../../../application/book.c ../../../application/hashtables.c ../../../application/eval.c ../../../application/move_gen.c ../../../application/search.c
cd ..
cd ./bin/
rm ./*.bin >&/dev/null
rm ./*.elf >&/dev/null
rm ./*.hex >&/dev/null
rm ./*.map >&/dev/null
echo ""
echo "**********"
echo "linking..."
echo "**********"
echo ""
"$compiler" -O2 -mcpu=cortex-m4 -mtune=cortex-m4 -mthumb -nostartfiles -nodefaultlibs -nostdlib -ffreestanding -Wl,-Map=ct800fw.map -Wl,--entry=main -Wl,--gc-sections -Wl,-T../../../arm-gcc-link.ld -o ct800fw.elf ../obj/boot_stm32f405.o ../obj/arm_driver.o ../obj/hardware_arm.o ../obj/hardware_arm_disp.o ../obj/hardware_arm_keybd.o ../obj/hardware_arm_signal.o ../obj/util.o ../obj/hmi.o ../obj/timekeeping.o ../obj/play.o ../obj/menu.o ../obj/posedit.o ../obj/move_gen.o ../obj/kpk.o ../obj/book.o ../obj/hashtables.o ../obj/eval.o ../obj/search.o
# if the linkage has failed, abort here.
if [ ! -f "./ct800fw.elf" ]; then
    echo ""
    echo "**************************"
    echo "could not link executable."
    echo "build failed.             "
    echo "**************************"
    echo ""
    # go back to the starting directory
    cd "$starting_dir"
    read -n1 -r -p "press any key to continue..." key
    exit 1
fi
echo "executable info:"
"$obj_size" ./ct800fw.elf
echo ""
echo "****************"
echo "appending crc..."
echo "****************"
"$obj_cpy" -O binary ./ct800fw.elf ./ct800fw.bin
../../../tool_bin/crctool ct800fw.bin 0x08000000
cp ./ct800fw_crc.hex ../../../CT800FW_CRC_${fw_ver}.hex
cp ./ct800fw_crc.bin ../../../CT800FW_CRC_${fw_ver}.bin
cp ./ct800fw.map ../../../CT800FW_${fw_ver}.map
# go back to the starting directory
cd "$starting_dir"
echo ""
echo "*************************"
echo "OK: CT800FW_CRC_${fw_ver}.bin"
echo "OK: CT800FW_CRC_${fw_ver}.hex"
echo "done.                    "
echo "*************************"
echo ""
read -n1 -r -p "press any key to continue..." key