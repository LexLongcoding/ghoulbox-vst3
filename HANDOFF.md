# Handoff: Ghoulbox as a VST3 in Cubase

Goal: run the Ghoulbox firmware (jasonpersinger/ghoulbox-fm1-dungeon-synth, a Felucca fork
for the M-VAVE FM-1, GPL-3.0) as a VST3 instrument in Cubase, keeping its GUI.
A browser version exists (https://ghoulbox.messy.quest) and works.

## Approach (decided)
Native JUCE plugin. The repo's browser emulator `web/emu/felucca_web.c` is ONE C translation unit
that wraps the whole firmware against a simulated FM-1. `Source/gb_host.c` #includes it UNCHANGED
and wraps its `web_*` exports as a small C API (`Source/gb_host.h`, prefix `gbx_`).
Plugin shell (JUCE 8): audio out (resampled if host != firmware rate), MIDI in, panel UI drawn like
web/emu/index.html "wide" layout, LEDs, MASTER as automatable parameter, state = non-blank flash
sectors + live project (web_project_capture / web_project_restore).
Behaviours copied from web/emu/worklet.js: panel changes queued and held >= 24 ms device time;
render in 128-frame slices; MASTER set before boot.

## Status
Everything is WRITTEN BUT NEVER COMPILED OR RUN. Expect first-build errors.

## Build
1. In the Ghoulbox repo run `./build.sh` once (needs Linux; on Windows use WSL2; macOS needs Docker).
   Only `build/gen/` (tables, fonts, samples) is needed for the plugin; later stages of build.sh
   (JieLi firmware toolchain) may be ignorable. Needs: python3, `pip install Pillow fonttools`,
   `tools/get_toolchain.sh`, and the AC79 SDK clone (see the repo's BUILDING.md).
2. Plugin: see README.md. Windows needs clang-cl + Ninja (firmware is GNU C); mac/Linux normal.
   `cmake -B build -DGHOULBOX_ROOT=<repo path>` then `cmake --build build`.

## Known risks / what to check first
- Firmware compiled natively may need portability fixes (it includes tests/hostsim.c).
- The emulator build gives the firmware a 1 MiB stack; host audio threads may have less. If it
  crashes on playback: run the firmware on its own big-stack thread and pass audio via a ring.
- Screen colours wrong => remove the byte swap in `gbx_copy_screen` (gb_host.c).
- Multi-instance via per-instance copies of ghoulbox_fw (Source/GbFirmware.*). Windows: needs the stubs Source/unistd.h and Source/sys/wait.h.
- Not emulated: USB audio/MIDI, TRS MIDI, MIDI out, installer, power-on LED sweep.

## Files
CMakeLists.txt, README.md, Source/{gb_host.h, gb_host.c, PluginProcessor.h/.cpp, PluginEditor.h/.cpp}
