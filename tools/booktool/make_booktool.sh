#!/bin/bash
gcc -Wall -Wextra -Wlogical-op -Wstrict-prototypes -Werror -O2 -o booktool ./source/main.c ./source/check.c ./source/convert.c ./source/util.c