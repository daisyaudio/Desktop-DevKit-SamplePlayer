#pragma once
#include <cstdlib>
#include "daisy.h"
#include "daisysp.h"
#include "distortion_effect.h"
#include "filter_effect.h"

/** maximum number of steps in a sequence */
constexpr size_t kMaxSequenceSteps = 8;

/** number of voices for sample playback */
constexpr size_t kMaxVoices = 8;

constexpr size_t kTransferWorkspacePerVoice = 16384;

/** Multi-voice sample player
 *
 *  Opens the first 8 WAV files from a connected SD card
 *  and prepares them all for simultaneous playback.
*/
struct SamplePlayer {
    /** Looks in the root directory of a drive, and loads samples into wav players
     *  assumes that the SD Card config, and FatFS connections have already been
     *  made.
    */
    inline void Init() { ReloadFiles(); }

    /** Search the root-directory of the attached SD Card, and load up to 8 WAV files.
     *  Report which files were loaded, in order, into "samples-loaded.txt"
     */
    inline void ReloadFiles() {
        file_table.Fill("/", ".wav");
        file_table.WriteLog("samples-loaded.txt");
        for(size_t i = 0; i < file_table.GetNumFiles(); i++) {
            voice[i].Init(file_table.GetFileName(i));
            // If files were playing back already, this can prevent
            // overruns when loading new files, etc.
            voice[i].Restart();
            voice[i].SetPlaying(false);
        }
    }

    inline void Trigger(size_t index) { voice[index].Restart(); }

    inline void Stop(size_t index) { voice[index].SetPlaying(false); }

    daisy::WavPlayer<kTransferWorkspacePerVoice> voice[kMaxVoices];
    daisy::FileTable<kMaxVoices>                 file_table;

    /** type alias for simpler result handling */
    using PlayerResult = daisy::WavPlayer<kTransferWorkspacePerVoice>::Result;
};

/** Returns a human-readable string for each error message. */
const char* PlayerResultToStr(SamplePlayer::PlayerResult& res) {
    switch(res) {
        case SamplePlayer::PlayerResult::Ok: return "Ok";
        case SamplePlayer::PlayerResult::FileNotFoundError:
            return "FileNotFound";
        case SamplePlayer::PlayerResult::PlaybackUnderrun:
            return "PlaybackUnderrun";
        case SamplePlayer::PlayerResult::PrepareOverrun:
            return "PrepareOverrun";
        case SamplePlayer::PlayerResult::NewSamplesRequested:
            return "NewSamplesRequested";
        case SamplePlayer::PlayerResult::DiskError: return "DiskError";
    }
    return "Unknown";
}

/** Multi-sample playback sequencer.
 */
struct SampleSequencer {
    /** Direction that the sequencer's current step will move whenever Advance is called */
    enum class PlaybackDirection {
        Forward,
        Reverse,
        Random,
    };

    /** Initialize the sequencer.
     *
     *  This opens all files for streaming, seeds the random playback direction, and initializes the internal clock and performance effects.
     */
    void Init(float sample_rate) {
        // Seed random (used for oplayback direction)
        // could replace with any of the cpp <random> generators.
        // We use the Daisy's TRNG peripheral as a seed to avoid
        // the sequence being identical every time.
        daisy::Random::Init();
        std::srand(daisy::Random::GetValue());

        internal_clock.Init(2.f, sample_rate);
        direction = PlaybackDirection::Forward;
        play = prev_play = false;
        step             = 0;

        hold      = false;
        held_step = step;

        // We don't load files right now because the SD card may not be present yet.

        /** Initialize all steps to off by default */
        for(size_t i = 0; i < kMaxVoices; i++) {
            for(size_t j = 0; j < kMaxSequenceSteps; j++) {
                step_enabled[i][j] = false;
            }
        }

        /** Initialize the effects */
        for(size_t chn = 0; chn < 2; chn++) {
            distortion[chn].Init();
            distortion[chn].SetDrive(0.0);
            filter[chn].Init(sample_rate);
            filter[chn].SetCutoffParam(0.5f);
            decim[chn].Init();
            decim[chn].SetSmoothCrushing(true);
        }
    }

    inline void ReloadFiles() { player.ReloadFiles(); }

    /** Enable the specified step regardless of current state */
    inline void EnableStep(size_t voice, size_t pos) {
        step_enabled[voice][pos] = true;
    }

    /** Disable the specified step regardless of current state */
    inline void DisableStep(size_t voice, size_t pos) {
        step_enabled[voice][pos] = false;
    }

    /** Toggle the state of the specified step */
    inline void ToggleStep(size_t voice, size_t pos) {
        if(step_enabled[voice][pos]) {
            DisableStep(voice, pos);
        } else {
            EnableStep(voice, pos);
        }
    }

    /** Stops any ongoing playback of a specific voice */
    inline void StopVoice(size_t voice) { player.Stop(voice); }

    /** Returns true if the hold feature is enabled */
    inline bool GetHold() const { return hold; }

    /** Sets the state of the hold feature.
     *  When true, the sequence will not advance, but will repeat the current step
     */
    inline void SetHold(const bool state) {
        bool update_held = state && !hold;
        hold             = state;
        if(update_held) {
            held_step = step;
        }
    }

    /** Toggle the state of the hold feature.
     *  When true, the sequence will not advance, but will repeat the current step
     */
    inline void ToggleHold() {
        if(GetHold())
            SetHold(false);
        else
            SetHold(true);
    }


    /** Return whether the step for the specified voice is enabled */
    inline bool GetStepEnabled(size_t voice, size_t pos) const {
        return step_enabled[voice][pos];
    }

    /** Increment the current step of the sequence according to the current playback direction. */
    void Advance() {
        switch(direction) {
            case PlaybackDirection::Forward:
                step = (step + 1) % kMaxSequenceSteps;
                break;
            case PlaybackDirection::Reverse:
                if(step == 0) {
                    step = kMaxSequenceSteps - 1;
                } else {
                    step -= 1;
                }
                break;
            case PlaybackDirection::Random:
                step = rand() % kMaxSequenceSteps;
                break;
        }
    }

    /** Return the current step of the sequencer */
    inline size_t GetCurrentStep() const { return step; }

    /** Process the sequencer data, incrementing the step position, and triggering sample voices for active steps.
     *  Audio for up to 8 simultaneous samples are streamed from disk, and summed together.
     *  The sum of all audio are then run through a few performance effects.
     *
     *  @return a sample player result corresponding to the last non-OK status that occurred during the block of audio
     *  This is frequently underruns for a few milliseconds when samples are triggered (due to the latency of the disk).
     *  This could be improved.
     */
    inline SamplePlayer::PlayerResult
    Process(daisy::AudioHandle::OutputBuffer out, size_t size) {
        auto result = SamplePlayer::PlayerResult::Ok;
        if(play != prev_play) {
            internal_clock.Reset();
        }
        prev_play = play;

        for(size_t i = 0; i < size; i++) {
            if(internal_clock.Process() && play) {
                // Update sequence position
                Advance();
                // Use the held step if hold is active, otherwise keep playing
                auto step_to_trig = hold ? held_step : step;
                // Fire samples
                for(size_t v = 0; v < kMaxVoices; v++) {
                    if(step_enabled[v][step_to_trig]) {
                        player.Trigger(v);
                    }
                }
            }
            float out_sum[2] = {0.f};
            for(auto& v : player.voice) {
                float tout[2] = {0.f};

                auto stream_res = v.Stream(tout, 2);
                if(stream_res != SamplePlayer::PlayerResult::Ok) {
                    result = stream_res;
                }

                // Sum successive voices and hard-clip the result
                out_sum[0] = daisysp::fclamp(out_sum[0] + tout[0], -1.f, 1.f);

                // If the loaded file is mono, copy the data to the second channel.
                if(v.GetChannels() == 1) {
                    out_sum[1] = out_sum[0];
                } else {
                    out_sum[1]
                        = daisysp::fclamp(out_sum[1] + tout[1], -1.f, 1.f);
                }
            }
            /**
             *  BitCrush->Bidirectional Filter->Distortion->Output
             */
            auto crush_l = decim[0].Process(out_sum[0]);
            auto crush_r = decim[1].Process(out_sum[1]);
            auto filt_l  = filter[0].Process(crush_l);
            auto filt_r  = filter[1].Process(crush_r);
            auto dist_l  = distortion[0].Process(filt_l);
            auto dist_r  = distortion[1].Process(filt_r);
            out[0][i]    = dist_l;
            out[1][i]    = dist_r;
        }
        return result;
    }

    /** Acquires new samples for the sample player voices to stream.
     *  This should be run in the main while() loop or some other interruptable
     *  location.
     */
    inline SamplePlayer::PlayerResult Prepare() {
        auto result = SamplePlayer::PlayerResult::Ok;
        for(auto& v : player.voice) {
            auto _res = v.Prepare();
            if(_res != SamplePlayer::PlayerResult::Ok) {
                result = _res;
            }
        }
        return result;
    }

    /** Sets the filter effect parameter.
     *
     *  Values between 0.0 and 0.49 act as a resonant low-pass
     *  Values between 0.51 and 1.0 act as a resonant high-pass
     *  Values within +/- 0.01 of 0.5 will result in the filter being disabled.
     */
    void SetFilter(const float in) {
        // TODO: set filt freqs and/crossfade values
        filter[0].SetCutoffParam(in);
        filter[1].SetCutoffParam(in);
    }

    /** Adds gain/distortion to the final output of the signal chain */
    void SetDistortionAmount(const float amt) {
        distortion[0].SetDrive(amt);
        distortion[1].SetDrive(amt);
    }

    void SetBitcrushAmount(const float amt) {
        auto factor = amt * 0.7f;
        auto crush  = amt * 0.3f;
        decim[0].SetDownsampleFactor(factor);
        decim[0].SetBitcrushFactor(crush);
        decim[1].SetDownsampleFactor(factor);
        decim[1].SetBitcrushFactor(crush);
    }


    PlaybackDirection direction;
    bool              play;
    bool              step_enabled[kMaxVoices][kMaxSequenceSteps];
    size_t            step;

    SamplePlayer   player;
    daisysp::Metro internal_clock;
    bool           prev_play; //< used to track state transitions

    /** if true, repeat the same step until the setting is disabled.
     *  The sequence keeps in the background to stay more musical.
     */
    bool   hold;
    size_t held_step;

    // Performance Effects
    DistortionEffect   distortion[2];
    FilterEffect       filter[2];
    daisysp::Decimator decim[2];
};
