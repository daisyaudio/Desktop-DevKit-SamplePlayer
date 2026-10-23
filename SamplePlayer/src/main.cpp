/** Desktop DevKit Sample Player
 *
 *  Demo program for the Desktop DevKit
 *  8-voice xox-style sample playing sequencer.
 */
#include "desktop_devkit.h"
#include "userinterface.h"
#include "app_data.h"

using namespace daisy;
using namespace daisy::desktop_devkit;

using Log = Logger<LOGGER_EXTERNAL>;

// Global objects

Hardware        hw;
Ui              ui;
SampleSequencer sample_seq;
SdmmcHandler    sdmmc;
FatFSInterface  fsi;

/** Flags to trigger mounting/unmounting the SD card */
static bool     sd_unmount_flag, sd_remount_flag;
static uint32_t sd_remount_retries = 0;

CpuLoadMeter cpu_meter_dsp;

/** FIFO of strings that we can use to move messages from the Audio callback
 * to be printed in the main while() loop.
 */
FIFO<FixedCapStr<64>, 16> msg_fifo;

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size) {
    cpu_meter_dsp.OnBlockStart();
    hw.UpdateAllControls();
    ui.GenerateEvents();

    // Trigger unmount/remount of SD card based on its removal/insertion
    if(hw.sd_detect.FallingEdge()) {
        sd_unmount_flag = true;
        // If we're still retrying, stop.
        sd_remount_flag = false;
    } else if(hw.sd_detect.RisingEdge()) {
        sd_remount_flag    = true;
        sd_remount_retries = 0;
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
    if(res != SamplePlayer::PlayerResult::Ok
       && res != SamplePlayer::PlayerResult::NewSamplesRequested
       && res != SamplePlayer::PlayerResult::InitialLatency
       && res != SamplePlayer::PlayerResult::FileNotFoundError) {
        FixedCapStr<64> err("Playback err: ");
        err.Append(PlayerResultToStr(res));
        msg_fifo.PushBack(err);
    }
    cpu_meter_dsp.OnBlockEnd();
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
    Log::StartLog(false);

    /** SD Card / FatFS Interface Init */
    SdmmcHandler::Config sdcfg;
    sdcfg.Defaults();
    sdcfg.speed = SdmmcHandler::Speed::STANDARD;
    sdcfg.width = SdmmcHandler::BusWidth::BITS_4;
    sdmmc.Init(sdcfg);

    cpu_meter_dsp.Init(hw.seed.AudioSampleRate(), hw.seed.AudioBlockSize());

    sample_seq.Init(hw.seed.AudioSampleRate());
    AppData app_data = {&sample_seq, 0};
    ui.Init(&hw, &app_data);

    hw.StartLowPriorityCallback(LowPriorityCallback, 250);
    hw.seed.StartAudio(AudioCallback);

    uint32_t cpu_load_print_time = System::GetNow();

    while(true) {
        auto now = System::GetNow();

        // Handle SD Card unmounting after removal is detected:
        if(sd_unmount_flag) {
            // unmount and delink filesystem from hardware
            Log::PrintLine("Unmounting SD Card.");
            f_mount(nullptr, "/", 0);
            fsi.DeInit();
            // clear the flag when we're done (i'm fairly certain this cannot fail)..
            Log::PrintLine("unmount finished.");
            sd_unmount_flag = false;
        }

        // Remount SD Card after it is inserted.
        if(sd_remount_flag) {
            if(sd_remount_retries > 0) {
                Log::PrintLine("\tretrying...attempt: %d", sd_remount_retries);
                System::Delay(50);
            } else {
                Log::PrintLine("Remounting SD Card.");
            }
            // Perform entire de-init/re-init sequence
            f_mount(nullptr, "/", 0);
            fsi.DeInit();
            sdmmc.Reset();
            fsi.Init(FatFSInterface::Config::Media::MEDIA_SD);

            if(f_mount(&fsi.GetSDFileSystem(), "/", 1) != FR_OK) {
                Log::PrintLine("\tremount failed...");
                sd_remount_retries++;
                if(sd_remount_retries > 10) {
                    Log::PrintLine(
                        "\tmax retries exceeded.. remove and reinsert card to "
                        "re-attempt.");
                    sd_remount_flag = false;
                }
            } else {
                // Reload files
                sample_seq.ReloadFiles();

                // clear the flag when we're done.
                Log::PrintLine("\tremount succeeded.");
                sd_remount_flag = false;
            }
        }

        // Acquire new samples from the SD card to feed the audio
        auto res = sample_seq.Prepare();
        if(res != SamplePlayer::PlayerResult::Ok
           && res != SamplePlayer::PlayerResult::NewSamplesRequested) {
            FixedCapStr<64> err("Playback err: ");
            err.Append(PlayerResultToStr(res));
            Log::PrintLine(err.Cstr());
        }

        // Print if the DSP usage gets too high
        if(cpu_meter_dsp.GetAvgCpuLoad() > 0.89f
           && now - cpu_load_print_time > 500) {
            Log::PrintLine("High CPU usage: %d%",
                           (int)(cpu_meter_dsp.GetAvgCpuLoad() * 100.f));
            cpu_load_print_time = now;
        }

        while(!msg_fifo.IsEmpty()) {
            auto msg = msg_fifo.PopFront();
            Log::PrintLine(msg.Cstr());
        }
    }

    return 0;
}