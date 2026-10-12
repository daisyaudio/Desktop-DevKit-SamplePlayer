#pragma once
#include <cstdlib>
#include "daisy.h"
#include "daisysp.h"

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

    inline void ReloadFiles() {
        file_table.Fill("/", ".wav");
        file_table.WriteLog("samples-loaded.txt");
        for(size_t i = 0; i < file_table.GetNumFiles(); i++) {
            voice[i].Init(file_table.GetFileName(i));
        }
    }

    inline void Trigger(size_t index) {
        if(voice[index].GetPlaying()) {
            voice[index].Restart();
        } else {
            voice[index].SetPlaying(true);
        }
    }

    daisy::WavPlayer<kTransferWorkspacePerVoice> voice[kMaxVoices];
    daisy::FileTable<kMaxVoices>                 file_table;

    /** type alias for simpler result handling */
    using PlayerResult = daisy::WavPlayer<kTransferWorkspacePerVoice>::Result;
};

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

    void Init(float sample_rate) {
        // Seed random (used for oplayback direction)
        // could replace with any of the cpp <random> generators
        // we use the Daisy's TRNG peripheral as a seed to avoid
        // the sequence being identical every time.
        daisy::Random::Init();
        std::srand(daisy::Random::GetValue());
        player.Init();

        internal_clock.Init(2.f, sample_rate);
    }

    inline void EnableStep(size_t voice, size_t pos) {
        step_enabled[voice][pos] = true;
    }
    inline void DisableStep(size_t voice, size_t pos) {
        step_enabled[voice][pos] = false;
    }
    inline void ToggleStep(size_t voice, size_t pos) {
        if(step_enabled[voice][pos]) {
            DisableStep(voice, pos);
        } else {
            EnableStep(voice, pos);
        }
    }

    inline bool GetStepEnabled(size_t voice, size_t pos) const {
        return step_enabled[voice][pos];
    }

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

    inline size_t GetCurrentStep() const { return step; }

    inline SamplePlayer::PlayerResult
    Process(daisy::AudioHandle::OutputBuffer out, size_t size) {
        auto result = SamplePlayer::PlayerResult::Ok;
        if(play != prev_play) {
            internal_clock.Reset();
        }
        prev_play = play;

        for(size_t i = 0; i < size; i++) {
            if(internal_clock.Process() && play) {
                // Advance
                Advance();
                // Fire samples
                for(size_t v = 0; v < kMaxVoices; v++) {
                    if(step_enabled[v][step]) {
                        player.Trigger(v);
                    }
                }
            }
            float out_sum[2] = {0.f};
            for(auto& v : player.voice) {
                float tout[2] = {0.f};

                auto _res = v.Stream(tout, 2);
                // pass results back to the user to indicate underruns/etc.?
                if(_res != SamplePlayer::PlayerResult::Ok) {
                    result = _res;
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
            out[0][i] = out_sum[0];
            out[1][i] = out_sum[1];
        }
        return result;
    }

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

    inline void SetAmplitude(size_t voice, float amp) {
        amplitude[voice] = amp;
    }

    PlaybackDirection direction;
    bool              play;
    bool              step_enabled[kMaxVoices][kMaxSequenceSteps];
    size_t            step;
    float             amplitude[kMaxVoices];

    SamplePlayer   player;
    daisysp::Metro internal_clock;
    bool           prev_play; //< used to track state transitions
};

struct SampleManager {
    // stuff
};
