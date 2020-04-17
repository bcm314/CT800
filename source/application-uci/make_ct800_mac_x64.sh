#!/bin/bash
fw_ver="V1.40"
clang -DTARGET_BUILD=64 -DNO_MONO_COND -m64 -pthread -Wall -Wextra -Wstrict-prototypes -Werror -O02 -flto -std=c99 -fno-strict-aliasing -fno-strict-overflow -o output/CT800_${fw_ver}_x64 play.c kpk.c eval.c move_gen.c hashtables.c search.c util.c book.c
strip output/CT800_${fw_ver}_x64
