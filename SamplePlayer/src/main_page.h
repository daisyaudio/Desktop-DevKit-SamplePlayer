#pragma once

#include "daisy.h"
#include "app_data.h"

class MainPage : public daisy::UiPage {
  public:
    /** Simple alias for logging
     *  StartLog() for the destination should have already been called before the
     * page is opened.
     */
    using Log = daisy::Logger<daisy::LOGGER_EXTERNAL>;

    void Init(AppData *data) { data_ = data; }

    /** Update LEDs to show the state of the sequencer. */
    void Draw(const daisy::UiCanvasDescriptor &canvasDescriptor) override {
        auto *hw = reinterpret_cast<daisy::desktop_devkit::Hardware *>(
            canvasDescriptor.handle_);
        auto now = daisy::System::GetNow();
        hw->seed.SetLed((now & 511) < 255);

        float track_brightness[kMaxSequenceSteps] = {0.f};
        for(size_t i = 0; i < kMaxSequenceSteps; i++) {
            if(data_->sequencer->GetCurrentStep() == i) {
                track_brightness[i] = 1.f;
            } else if(data_->sequencer->GetStepEnabled(data_->selected_voice,
                                                       i)) {
                track_brightness[i] = 0.125f;
            }
        }
        hw->SetLed(data_->selected_voice, 1.f);
        for(size_t i = 0; i < 8; i++) {
            hw->SetLed(8 + i, track_brightness[i]);
        }
    }

    bool OnButton(uint16_t buttonID,
                  uint8_t  numberOfPresses,
                  bool     isRetriggering) override {
        switch(numberOfPresses) {
            case 0:
                // On release:
                if(buttonID < 8) {
                    // Select voice
                    data_->selected_voice = buttonID;
                } else {
                    auto step = buttonID - 8;
                    data_->sequencer->ToggleStep(data_->selected_voice, step);
                }
                break;
            case 1:
                // On Press:
                //Log::PrintLine("Pressed %d", buttonID + 1);
                break;
            case 2:
                // Long Press:
                // Top Row: stop playback of any ongoing playback of this voice
                if(buttonID < 8) {
                    data_->sequencer->StopVoice(buttonID);
                }
                break;
            default: break;
        }
        return false;
    }

    bool OnPotMoved(uint16_t potID, float newPosition) {
        // Print Format: "Pot {potID+1}: {newPosition}"
        daisy::FixedCapStr<16> valueStr("Pot ");
        valueStr.AppendInt(potID + 1);
        valueStr.Append(": ");
        valueStr.AppendFloat(newPosition, 3);
        Log::PrintLine(valueStr.Cstr());

        // Do things with the knobs:
        switch(potID) {
            case 0: {
                // Knob 1: Tempo
                auto new_freq = (0.5f + (newPosition * newPosition * 16.f));
                data_->sequencer->internal_clock.SetFreq(new_freq);
                break;
            }
            case 1: {
                // Knob 2: Step repeat
                data_->sequencer->SetHold(newPosition > 0.5f);
            } break;
            case 2: {
                // Knob 3: Filter Effect
                data_->sequencer->SetFilter(newPosition);
            } break;
            case 3: {
                // Knob 4: Distortion Effect
                data_->sequencer->SetDistortionAmount(newPosition);
            } break;
            case 4: {
                // Knob 5: Bitcrush Effect
                data_->sequencer->SetBitcrushAmount(newPosition);
            } break;
            case 5: {
                // Knob 6: Reverb Time
            } break;
            case 6: {
                // Knob 7: Reverb Amount
            } break;
            // Knobs 7 & 8: Add more knob features here :)
            default: break;
        }
        return false;
    }

    void OnShow() { Log::PrintLine("Page Opened!"); }

    AppData *data_;
};
