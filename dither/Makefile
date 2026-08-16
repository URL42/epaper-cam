# PaperCam — native build of the image-pipeline modules and their harnesses.
#
#   make test     build and run the dither property tests
#   make tune     build the tone-curve playground
#   make sync     copy the modules into each sketch's src/
#   make verify   fail if any sketch copy has drifted
#   make clean    remove binaries and generated images
#
# dither.c and tone.c compile unchanged inside the sketch; nothing else here
# is used by the Arduino build.

CC     ?= cc
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion

# Arduino only compiles sources inside the sketch folder, so each sketch that
# needs these modules carries a copy under its src/. This directory is the
# canonical one. Editing a sketch copy directly and then wondering why the
# tests still pass is the failure `verify` guards against.
MODULES  := dither.c dither.h tone.c tone.h
SKETCHES := ../PaperCam

.PHONY: all test clean sync verify

all: test_dither tune

test: test_dither
	./test_dither

test_dither: test_dither.c dither.c dither.h
	$(CC) $(CFLAGS) -o $@ test_dither.c dither.c

tune: tune.c dither.c tone.c dither.h tone.h
	$(CC) $(CFLAGS) -o $@ tune.c dither.c tone.c -lm

sync:
	@for d in $(SKETCHES); do \
		mkdir -p $$d/src && cp $(MODULES) $$d/src/ && echo "synced -> $$d/src"; \
	done

verify:
	@fail=0; for d in $(SKETCHES); do \
		for m in $(MODULES); do \
			if ! diff -q $$m $$d/src/$$m >/dev/null 2>&1; then \
				echo "OUT OF SYNC: $$d/src/$$m — run 'make sync'"; fail=1; \
			fi; \
		done; \
		[ $$fail -eq 0 ] && echo "in sync: $$d/src"; \
	done; exit $$fail

clean:
	rm -f test_dither tune *.pbm *.pgm
