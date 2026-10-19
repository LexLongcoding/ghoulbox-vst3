Ghoulbox VST3 (JUCE)

An unofficial VST3 instrument that runs the real Ghoulbox firmware for the M-VAVE FM-1, with its 240x240 screen and panel drawn in the plugin window. It compiles the browser emulator's web/emu/felucca_web.c unchanged (the same file the web build uses), so the sound and behaviour match the browser version at https://ghoulbox.messy.quest.

License: GPL-3.0-only, like Ghoulbox.

Status: built and tested in Cubase on Windows 11 x64, including two instances running side by side. Everything else (macOS, Linux, other hosts, many instances at once) is untested.

Credits
Ghoulbox: https://github.com/jasonpersinger/ghoulbox-fm1-dungeon-synth. This plugin is just a shell around it.
Felucca, the firmware Ghoulbox is forked from, by Leo Kuroshita (@kurogedelic), Hugelton Instruments.
The built-in instrument samples are CC0, from Versilian Studios (see assets/samples-cc0/ATTRIBUTION.txt in the Ghoulbox repo).
Built with JUCE 8.0.4.

This project is not affiliated with or endorsed by M-VAVE, the Ghoulbox author or the Felucca author.

Install (Windows x64, prebuilt)
Download the zip from the Releases page and unzip it.
Copy the whole Ghoulbox.vst3 folder to C:\Program Files\Common Files\VST3\. It must contain Contents\x86_64-win\Ghoulbox.vst3 and Contents\x86_64-win\ghoulbox_fw.dll. If you copy only the inner file, the plugin shows "Firmware library missing".
Rescan plugins in your DAW. If it won't load, install the Microsoft Visual C++ Redistributable (x64).
Build from source (Windows)

You need the Ghoulbox repo and its generated headers (build/gen), plus a Windows toolchain.

1. Generate the firmware headers (Linux or WSL2). You do not need the JieLi toolchain or SDK for this.

sudo apt install -y git python3 python3-venv python3-pip build-essential
python3 -m venv ~/gbvenv && source ~/gbvenv/bin/activate
pip install Pillow fonttools
git clone https://github.com/jasonpersinger/ghoulbox-fm1-dungeon-synth
cd ghoulbox-fm1-dungeon-synth
python3 tools/build.py --gen-only

This creates build/gen. (The repo's full ./build.sh also builds the real firmware and needs the JieLi toolchain; the plugin does not.) Copy the whole repo (without .git) to a normal Windows folder, for example C:\dev\ghoulbox.

2. Install the Windows tools. Visual Studio 2022 Build Tools with "Desktop development with C++" and "C++ Clang tools for Windows", plus CMake, Ninja and Git. Use a Clang from 2022 or newer: the Clang 12 that ships with the 2019 Build Tools fails to link JUCE.

3. Build, in the x64 Native Tools Command Prompt for VS 2022:

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl ^
  -DGHOULBOX_ROOT=C:/dev/ghoulbox
cmake --build build

JUCE is downloaded by CMake. The plugin is built in build\Ghoulbox_artefacts\Release\VST3\Ghoulbox.vst3 and copied to your VST3 folder if the prompt has administrator rights (otherwise the build ends with a copy error; copy the folder by hand).

Windows has no unistd.h or sys/wait.h, which the firmware's host-test code includes, so Source/unistd.h and Source/sys/wait.h are small stand-ins. They only exist to make that code compile.

macOS and Linux: cmake -B build -DCMAKE_BUILD_TYPE=Release -DGHOULBOX_ROOT=/path/to/ghoulbox, then cmake --build build. Not tested.

Multiple instances

The firmware keeps its state in C globals, so compiled into the plugin it would be one synth shared by every instance. Instead it is built as its own library (ghoulbox_fw.dll, next to the plugin binary in the bundle). Each instance copies that library to a uniquely named file in the temp folder (GhoulboxFW) and loads the copy, so each instance has its own globals and is a separate synth. Copies are removed when the instance is closed, and stale ones are swept at the next start.

How it maps
Browser build export	Plugin
web_render	audio out (resampled if the host rate differs)
web_midi	MIDI in (channel messages)
web_keys / web_buttons / web_enc	on-screen keys, buttons, knobs
web_master	plugin parameter "Master" (automatable)
web_screen	plugin window (byte-swapped RGB565 -> normal)
web_nor + web_project_*	saved in the host project
Panel

The editor follows the browser page's "wide" drawing (1000 x 624, resizable): MASTER / SELECT / PRESETS / ALGORITHM on the left, the screen, KNOB 1-4 and the 12 + 2 buttons on the right, the 27 keys below.

Computer keys, as in the browser: Z X C V B N M , . / and S D F H J L ; ' for F3 upward, Q..O for the top keys, 1 2 3 4 5 6 7 8 9 0 - = for the buttons (FX .. REC), [ ] for OCT- / OCT+, arrows for SELECT and ALGORITHM. Shift-click (or right-click) a button to latch it (for example GLO + a key). Knobs: drag up/down or use the mouse wheel. MASTER is an automatable plugin parameter.

Known limits
Windows x64 tested only.
Not emulated (the firmware's browser build doesn't emulate them either): USB audio and MIDI, TRS MIDI, MIDI out, the installer.
The power-on LED sweep isn't drawn; LEDs appear normally once it ends.
The firmware runs on the host's audio thread, which may have a smaller stack than the 1 MiB the browser build gives it. If a host crashes on playback, the fix is to run the firmware on its own big-stack thread and pass audio through a ring buffer.
If screen colours look wrong, remove the byte swap in gbx_copy_screen (Source/gb_host.c).
Saved state is every non-blank flash sector (settings, projects, user presets, autosave) plus the live music, stored in the host project.
