/** Desktop DevKit UI Template
 *
 *  UI template program for the Desktop DevKit
 *
 *  This program uses the libDaisy UI framework to generate events from the input
 *  that can be handled within UI pages.
 *  This is a bit more complex than the simpler approach to handling UI in other examples,
 *  but provides a lot of flexibility for more involved applications.
 *
 *  Since the Desktop DevKit is USB powered via the port on the devkit, this build uses the
 *  external variant of the bootloader so that the seed on the devkit can be reprogrammed
 *  while connected to the single USB port on the desktop devkit.
 *
 *  This program:
 *  - initializes the Desktop DevKit hardware
 *  - starts an audio callback to do pass through, update the controls, and generate UI events
 *  - starts a low-priority callback to process the UI events
 *  - prints input data from the `main_page.h` event hooks over USB serial
 */
#include "desktop_devkit.h"
#include "userinterface.h"
#include "app_data.h"

using namespace daisy;
using namespace daisy::desktop_devkit;

// Global objects

Hardware        hw;
Ui              ui;
SampleSequencer sample_seq;
SdmmcHandler    sdmmc;
FatFSInterface  fsi;

/** Flags to trigger mounting/unmounting the SD card */
static bool sd_unmount_flag, sd_remount_flag;

/** FIFO of strings that we can use to move messages from the Audio callback
 * to be printed in the main while() loop.
 */
FIFO<FixedCapStr<64>, 16> msg_fifo;

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size) {
    hw.UpdateAllControls();
    ui.GenerateEvents();

    // Trigger unmount/remount of SD card based on its removal/insertion
    if(hw.sd_detect.FallingEdge()) {
        sd_unmount_flag = true;
    } else if(hw.sd_detect.RisingEdge()) {
        sd_remount_flag = true;
    }

    // These could be handled in the UI, but for simplicity sake, the UI is
    // only generating events for the 16 buttons, and 8 pots.
    sample_seq.play = hw.tog_sw17.Pressed();
    switch(hw.tog_sw18.Read()) {
        case Switch3::POS_UP:
            sample_seq.direction = SampleSequencer::PlaybackDirection::Forward;
            break;
        case Switch3::POS_CENTER:
            sample_seq.direction = SampleSequencer::PlaybackDirection::Reverse;
            break;
        case Switch3::POS_DOWN:
            sample_seq.direction = SampleSequencer::PlaybackDirection::Random;
            break;
        default: break;
    }

    // Run the sequencer, filling the output buffer with samples.
    auto res = sample_seq.Process(out, size);
    (void)(res); //< temporarily unused

    // Report any unexpected results to be printed from the main loop
    // TODO: filter out the trigger latency underruns (a few ms of underrun
    // before new samples arrive)
    // if(res != SamplePlayer::PlayerResult::Ok
    //    && res != SamplePlayer::PlayerResult::NewSamplesRequested
    //    && res != SamplePlayer::PlayerResult::FileNotFoundError) {
    //     FixedCapStr<64> err("Playback err: ");
    //     err.Append(PlayerResultToStr(res));
    //     msg_fifo.PushBack(err);
    // }
}


/** This callback happens every 4ms (250Hz) and can be interrupted by
 * everything except for the SysTick interrupt.
 * The frequency of this callback is set in the `Hardware::StartLowPriorityCallback`
 * parameters.
 *
 * Handling the UI events here means that anything we do in the main while()
 * loop that may take an indeterminate time (file system I/O, QSPI Saving, etc.)
 * won't cause the UI to stall during those processes
*/
void LowPriorityCallback(void *data) {
    // Handles events, and redraws the current UI pages
    ui.DoEvents();
}

int main(void) {
    hw.Init();
    System::Delay(50);
    Logger<LOGGER_EXTERNAL>::StartLog(false);

    /** SD Card / FatFS Interface Init */
    SdmmcHandler::Config sdcfg;
    sdcfg.Defaults();
    sdcfg.speed = SdmmcHandler::Speed::FAST;
    sdcfg.width = SdmmcHandler::BusWidth::BITS_4;
    sdmmc.Init(sdcfg);

    // We could do this here, and then either get the initial
    // "remount" flag to clear itself, or we could just let it
    // do its thing.
    // fsi.Init(FatFSInterface::Config::Media::MEDIA_SD);
    // f_mount(&fsi.GetSDFileSystem(), "/", 1);

    sample_seq.Init(hw.seed.AudioSampleRate());
    AppData app_data = {&sample_seq, 0};
    ui.Init(&hw, &app_data);

    hw.StartLowPriorityCallback(LowPriorityCallback, 250);
    hw.seed.StartAudio(AudioCallback);

    // auto     last_dac_write = System::GetNow();
    // uint16_t dac_value      = 0;

    while(true) {
        // auto now = System::GetNow();

        // Handle SD Card un/remounting:
        if(sd_unmount_flag) {
            // unmount and delink filesystem from hardware
            Logger<LOGGER_EXTERNAL>::PrintLine("Unmounting SD Card.");
            f_mount(nullptr, "/", 0);
            fsi.DeInit();
            // clear the flag when we're done.
            Logger<LOGGER_EXTERNAL>::PrintLine("unmount finished.");
            sd_unmount_flag = false;
        }
        if(sd_remount_flag) {
            Logger<LOGGER_EXTERNAL>::PrintLine("Remounting SD Card.");
            // Reset SDMMC peripheral and relink FatFS
            sdmmc.Reset();
            fsi.Init(FatFSInterface::Config::Media::MEDIA_SD);
            f_mount(&fsi.GetSDFileSystem(), "/", 1);

            // Reload files
            sample_seq.player.ReloadFiles();

            // clear the flag when we're done.
            Logger<LOGGER_EXTERNAL>::PrintLine("remount finished.");
            sd_remount_flag = false;
        }

        auto res = sample_seq.Prepare();
        if(res != SamplePlayer::PlayerResult::Ok
           && res != SamplePlayer::PlayerResult::NewSamplesRequested) {
            FixedCapStr<64> err("Playback err: ");
            err.Append(PlayerResultToStr(res));
            Logger<LOGGER_EXTERNAL>::PrintLine(err.Cstr());
        }

        // while(!msg_fifo.IsEmpty()) {
        //     auto msg = msg_fifo.PopFront();
        //     Logger<LOGGER_EXTERNAL>::PrintLine(msg.Cstr());
        // }

        /** Handle MIDI events */
        // while(hw.midi.HasEvents()) {
        //     auto msg = hw.midi.PopEvent();
        //     switch(msg.type) {
        //         case NoteOn:
        //             // Echo note on messages to the MIDI out
        //             {
        //                 uint8_t bytes[3] = {0x90, 0x00, 0x00};
        //                 bytes[1]         = msg.data[0];
        //                 bytes[2]         = msg.data[1];
        //                 hw.midi.SendMessage(bytes, 3);
        //             }
        //             break;
        //         default: break;
        //     }
        // }

        // System::Delay(1);
    }

    return 0;
}