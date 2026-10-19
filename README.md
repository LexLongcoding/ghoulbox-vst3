# Ghoulbox VST3 (JUCE)

Runs the real Ghoulbox firmware inside a VST3 instrument, with its 240x240
screen drawn in the plugin window. It compiles the browser emulator's
`web/emu/felucca_web.c` **unchanged** (the same file the web build uses), so
the sound and behaviour match the browser version. GPL-3.0-only, like Ghoulbox.

STATUS: single-instance build compiled and tested in Cubase (Windows). The multi-instance loader is new and untested.

## Build

1. In the Ghoulbox repo, run `./build.sh` once (creates `build/gen`).
2. Windows needs **clang-cl + Ninja** (the firmware uses GNU C):

       cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
         -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl ^
         -DGHOULBOX_ROOT=C:/path/to/ghoulbox-fm1-dungeon-synth
       cmake --build build

   macOS / Linux:

       cmake -B build -DCMAKE_BUILD_TYPE=Release -DGHOULBOX_ROOT=/path/to/ghoulbox
       cmake --build build

3. The VST3 is copied to your VST3 folder. Rescan in Cubase.

## How it maps

| Browser build export | Plugin |
|---|---|
| `web_render` | audio out (resampled if the host rate differs) |
| `web_midi` | MIDI in (channel messages) |
| `web_keys` / `web_buttons` / `web_enc` | on-screen keys, buttons, knobs |
| `web_master` | plugin parameter "Master" (automatable) |
| `web_screen` | plugin window (byte-swapped RGB565 -> normal) |
| `web_nor` + `web_project_*` | saved in the Cubase project |

## Panel

The editor follows the browser page's "wide" drawing (1000 x 624, resizable):
MASTER / SELECT / PRESETS / ALGORITHM on the left, the screen, KNOB 1-4 and the
12 + 2 buttons on the right, the 27 keys below. Button ids and encoder roles are
the firmware's own (same as `web/emu/index.html`).

Computer keys, as in the browser: Z X C V B N M , . / and S D F H J L ; ' for
F3 upward, Q..O for the top keys, 1 2 3 4 5 6 7 8 9 0 - = for the buttons
(FX .. REC), [ ] for OCT- / OCT+, arrows for SELECT and ALGORITHM.
Shift-click (or right-click) a button to latch it (e.g. GLO + a key).
Knobs: drag up/down or mouse wheel. MASTER is an automatable plugin parameter.

## Still to check

- If screen colours look wrong, drop the byte swap in `gbx_copy_screen`
  (gb_host.c).
- The power-on LED sweep (`web_anim_levels`) isn't drawn; LEDs appear normally
  once it ends.

## Behaviour copied from the browser build (web/emu/worklet.js)

- Panel presses are queued and each change is held >= 24 ms of device time
  (the firmware reads the contacts once per UI frame), and the firmware is run
  in 128-frame slices like an AudioWorklet quantum.
- Buttons and keys show the firmware's LEDs: lit, dim, breathing, mid, PLAY green.
- MASTER is set before boot, as the browser does.

## Known limits and risks

- Multiple instances: the firmware is built as `ghoulbox_fw.(dll|so|dylib)` and each instance loads its own
  uniquely named temp copy, so their globals are separate. The library must sit next to the plugin binary
  inside the .vst3 bundle: always copy the whole `Ghoulbox.vst3` folder. Untested with many instances at once.
- No MIDI out, USB audio, TRS MIDI or installer (not emulated by the firmware
  build either).
- The emulator build gives the firmware a 1 MiB stack. The audio thread of a
  host usually has less; if Cubase crashes on playback, the fix is to run the
  firmware on its own thread with a large stack and pass audio through a ring.
- Saved state = every non-blank flash sector (settings, projects, user presets,
  autosave) + the live music. Reloading a project restores both.
