# PaperCam — native build of the dither module and its tests.
#
#   make test    build and run
#   make clean   remove the binary and generated images
#
# Nothing here is used by the Arduino build; dither.c and dither.h compile
# unchanged inside the sketch.

CC     ?= cc
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion

.PHONY: all test clean

all: test_dither

test: test_dither
	./test_dither

test_dither: test_dither.c dither.c dither.h
	$(CC) $(CFLAGS) -o $@ test_dither.c dither.c

clean:
	rm -f test_dither *.pbm *.pgm
