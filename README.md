# Desktop DevKit Sample Player

Demo program for the Daisy Desktop DevKit.

## App Overview

8-voice xox-style sample playing sequencer.

Up to 8 mono or stereo WAV files are streamed from an SD Card.

Files are loaded in alphabetical order, and the order can be checked by
removing the SD card, and reading "samples-loaded.txt" file.
The SD Card can be removed/re-inserted while the device is running.
However, because the application streams audio from the SD card, playback
will stop while the SD card is removed.

### Button Grid

The top row of buttons selects the voice.
Holding a button in this row for 1.5s or more will cause ongoing playback of
this voice to stop (can be useful if accidentally loading/triggering a long file).

The bottom row of buttons toggles whether the sample will trigger on that step
of the sequence.

The LEDs above each button indicate the currently selected voice, the sequence
pattern for that voice, and the current step of the sequence during playback.

### Toggles

**SW18 (3-way toggle)**: Controls playback direction of sequence.

- Up: playback direction is forward.
- Middle: playback direction is reversed.
- Down: playback direction is random.

**SW19 (2-way toggle)**: Play/Pause

- Up: Internal clock (rate set by the tempo knob) will advance the sequence.
- Down: Sequence is paused. New samples will not trigger, and the sequence
  will not advance

### Knobs

1. Tempo: Rate of the internal clock
2. Hold/Repeat: When set above half way, the current step will repeat.
   The sequence continues advancing so that this can be more rhythmically
   muscial than stalling the sequence altogether.
3. Bi-directional Filter: When set to 50% no filtering is applied.
   Below 50%, a resonant low-pass filter is applied
   Above 50%, a resonant high-pass filter is applied
4. Drive: Adds an overdrive distortion
5. Bitcrush
6. TBD
7. TBD
8. TBD

---

You'll notice there are a few unused knobs, and other controls.
These may get populated over time, but are also a great jumping
in point to add new features in your own version of the app!


## Get Started

If cloning this repo from Github, you will want to do so with the submodules with:

```console
git clone https://github.com/daisyaudio/Desktop-DevKit-SamplePlayer --recurse-submodules
```

if you've already cloned the repo without this you can fill the submodules by running the following:

```console
git submodule update --init --recursive
```

Once cloned, you can build the program using the VS code tasks described below, or by running the commands in the terminal.

```sh
# 1. Build libraries (only required the first time you build, or when updating libraries)
./ci/build_libs.sh

# 2. Build the application
make -C SamplePlayer
```

This application uses the external USB variant of the Daisy Bootloader to allow
programming the daisy through the same USB Port that you power the Desktop
DevKit without needing to connect two USB cables.

To install the program onto the Daisy, you can use the VS Code tasks or run the commands in the terminal.

```sh
# 1. With the USB Cable connected to the Daisy Seed, enter the system bootloader
# (Hold BOOT, and tap RESET)
make -C SamplePlayer program-boot

# 2. Move the USB cable to the Desktop DevKit's USB-C port and enter the daisy bootloader
# (Tap RESET, then tap BOOT -- you should see the LED blink and then start pulsing)
make -C SamplePlayer program-dfu
```

## Contents

Includes:

* libDaisy - hardware library for Daisy
* DaisySP - DSP library
* SamplePlayer - An 8-track sample-playing sequencer program for the Seed3 Desktop Devkit.

This also contains the following VS code tasks:

* Build Libraries: builds libDaisy, and DaisySP from scratch. This is only necessary when manually updating, or cloning for the first time.
* Build: build the blink example
* Clean: removes compiled Blink code
* Program DFU: programs the Blink.bin file via USB
* Build and Program DFU: Rebuilds the program and downloads it via USB DFU
* Install Bootloader: Flashes the bootloader to the Daisy via USB DFU

In addition, the following debug configurations are available:

* Debug SamplePlayer: debugs the SamplePlayer application with an ST-Link debug probe.
* Debug (J-Link): same as above, but for Segger J-Link debug probes.

**Note**: debugging an application from VS Code requires an ST-Link or similar probe, as well as the Cortex Debug extension for VS Code.

## Debugging

Debugging works similarly to other Daisy programs.
However, due to the use of the bootloader, the Debug launch command won't
actually program the new app in place.

So when making changes and debugging, make sure to flash the program over DFU
prior to launching the debug session.
Not doing so can result in unexpected behavior.