# AETHER — Build instructions

**AETHER — by Synth Centric**

## Requirements

| Tool | Version used for v1.0.0 | Purpose |
|---|---|---|
| GBDK-2020 | 4.3.0 (Linux x64 release) | SDCC-based C compiler, linker, `makebin`, `bankpack`, `romusage` |
| GNU make | any | build orchestration |
| Python 3 | 3.10+ | asset generators (`tools/mkgfx.py`, `tools/mktables.py`) |
| gcc | any C99 compiler | host test-suite and simulator |
| PyBoy | 2.x (`pip install pyboy`) + numpy | optional: emulator tests and demo recordings |
| pandoc + Chromium | optional | PDF manual |

## Install GBDK-2020

```bash
curl -L -o gbdk.tar.gz https://github.com/gbdk-2020/gbdk-2020/releases/download/4.3.0/gbdk-linux64.tar.gz
```
```bash
mkdir -p /opt/gbdk-dl && tar xzf gbdk.tar.gz -C /opt/gbdk-dl
```

The Makefile expects `GBDK_HOME=/opt/gbdk-dl/gbdk/` (override on the command line: `make GBDK_HOME=/path/to/gbdk/`).

## Build the ROM

```bash
make
```

Output: `build/AETHER_by_Synth_Centric.gb` (128 KB). The build prints a bank usage table. The generated sources `src/interface/gfx_data.c` (font, meters, logo) and `src/audio/tables.c` (pitch and noise tables) are committed; they are regenerated automatically when `assets/graphics/font5x7.txt` or the generator scripts change.

Linker settings (see `Makefile`): `-autobank` (GBDK bank packing), `-Wl-yt0x1B` (MBC5+RAM+BATTERY), `-Wl-ya4` (4 × 8 KB SRAM = 32 KB), `-Wm-yn"AETHER"` (header title). The CGB flag is left at 0x00 (original Game Boy).

## Tests

```bash
make test
```
Host test-suite (gcc): compiles the same generative-core sources used in the ROM and runs 20 test groups (~100 000 checks).

```bash
make emutest
```
Runs the ROM in PyBoy headless: title, navigation, editing, playback, IMPRINT, save → power cycle → load, a 60-minute playback and performance profiling. Results: `build/emu/report.md`, screenshots in `build/emu/`.

```bash
python3 tests/emu_test.py build/AETHER_by_Synth_Centric.gb --long 60 --demo 90
```
Also records 90 s of every factory composition from the ROM into `release/demo_audio/`.

Long simulations of the engine (host, deterministic):
```bash
gcc -std=c99 -O2 -Iinclude src/audio/audio_queue.c src/generative/*.c src/harmony/harmony.c src/ecology/ecology.c src/synthesis/*.c src/timing/form.c src/presets/factory.c src/storage/save.c tests/host_platform.c tests/simulate.c -o build/simulate
```
```bash
./build/simulate 5 60 - -v
```
(preset 0–8, minutes, seed or `-`, `-v` for a minute-by-minute trace).

## Release package

```bash
make release
```
Runs the tests, then `tools/package.sh`: copies the ROM, builds the PDF manual (when pandoc and Chromium/Playwright are available), the source archive and the release notes into `release/`.

## Running AETHER

* **Emulators:** any accurate DMG emulator with MBC5 battery-RAM support (SameBoy, BGB, Gambatte/SameBoy cores, mGBA). The project was tested in PyBoy.
* **Hardware:** a flash cartridge supporting MBC5 with at least 32 KB battery-backed SRAM (e.g. EverDrive GB, EZ-Flash Junior, GB USB 64M with an MBC5 32 KB SRAM profile). Not tested on physical hardware by the developer — see `LIMITATIONS.md`.

## Save compatibility

* Save format version **1**, payload 915 bytes, two checksummed copies per slot (see `ARCHITECTURE.md` §7).
* A future version with a different payload layout will report older slots as unreadable (version mismatch) rather than loading garbage; a migration routine would be added in `save.c` at that point. Slots written by v1.0.0 are only guaranteed to load in builds with `SAVE_FORMAT_VERSION 1` and `PROJECT_SIZE 915`.
* Uninitialised or foreign SRAM content is detected and never loaded.
