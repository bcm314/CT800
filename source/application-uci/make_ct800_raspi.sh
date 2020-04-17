#!/bin/bash
fw_ver="V1.40"
gcc -DTARGET_BUILD=32 -pthread -Wall -Wextra -Wlogical-op -Wstrict-prototypes -Werror -O02 -march=native -mtune=native -flto -std=c99 -faggressive-loop-optimizations -fno-unsafe-loop-optimizations -fgcse-sm -fgcse-las -fgcse-after-reload -fno-strict-aliasing -fno-strict-overflow -o output/CT800_${fw_ver}_x32 play.c kpk.c eval.c move_gen.c hashtables.c search.c util.c book.c -lrt -Wl,-s
