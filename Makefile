# AETHER - by Synth Centric
# Build:   make            -> build/AETHER_by_Synth_Centric.gb
#          make test       -> host test-suite (gcc)
#          make emutest    -> emulator tests (PyBoy, headless)
#          make release    -> release/ package
GBDK_HOME ?= /opt/gbdk-dl/gbdk/
LCC       = $(GBDK_HOME)bin/lcc
ROMNAME   = AETHER_by_Synth_Centric
ROM       = build/$(ROMNAME).gb

# MBC5 + RAM + BATTERY (0x1B), 4 x 8 KB SRAM banks, ROM banks auto-sized
LCCFLAGS  = -Iinclude
LINKFLAGS = -autobank -Wb-ext=.rel -Wl-yt0x1B -Wl-ya4 -Wm-yn"AETHER" -Wm-yk"SC" -Wl-m -Wl-j

SRCS := $(wildcard src/*.c src/*/*.c)
OBJS := $(patsubst src/%.c,build/obj/%.o,$(SRCS))

CORE := src/audio/audio_queue.c src/generative/rng.c src/generative/engine.c \
        src/generative/agents.c src/generative/motif.c src/harmony/harmony.c \
        src/ecology/ecology.c src/synthesis/modulation.c src/synthesis/patches.c \
        src/timing/form.c src/presets/factory.c src/storage/save.c

all: $(ROM)

src/interface/gfx_data.c: tools/mkgfx.py assets/graphics/font5x7.txt
	python3 tools/mkgfx.py $@

src/interface/help_data.c: tools/mkhelp.py assets/help.txt
	python3 tools/mkhelp.py $@

src/audio/tables.c: tools/mktables.py
	python3 tools/mktables.py $@

build/obj/%.o: src/%.c include/*.h
	@mkdir -p $(dir $@)
	$(LCC) $(LCCFLAGS) -c -o $@ $<

$(ROM): $(OBJS)
	$(LCC) $(LINKFLAGS) -o $@ $(OBJS)
	@$(GBDK_HOME)bin/romusage build/$(ROMNAME).map -g || true

test: build/tests
	./build/tests

build/tests: $(CORE) tests/test_main.c tests/host_platform.c include/*.h
	@mkdir -p build
	gcc -std=c99 -O2 -g -Wall -Wextra -Wno-unknown-pragmas -Wno-unused-parameter -Iinclude \
	    $(CORE) tests/host_platform.c tests/test_main.c -o $@

emutest: $(ROM)
	python3 tests/emu_test.py $(ROM)

release: $(ROM) test
	sh tools/package.sh

clean:
	rm -rf build/obj build/*.gb build/*.map build/*.noi build/*.ihx build/tests build/smoke

.PHONY: all test emutest release clean
