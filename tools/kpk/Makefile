CFLAGS=-std=c99 -pedantic -Wall -Wextra -Wlogical-op -Wno-cast-align -Werror -O2

all: pfkpk test

pfkpk: pfkpk.c kpk.c kpk.h
	$(CC) $(CFLAGS) -o $@ pfkpk.c kpk.c

test:
	./pfkpk

# vi: noexpandtab
