CC      = gcc
CFLAGS  = -Wall -Wextra -std=gnu11 -O2

all: banker

banker: banker.c ; $(CC) $(CFLAGS) -o $@ $<

test: banker ; sh tests/run_tests.sh

clean: ; rm -f banker result.txt

.PHONY: all clean test