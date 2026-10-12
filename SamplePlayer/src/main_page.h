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
                // Log::PrintLine("Released %d", buttonID + 1);
                if(buttonID < 8) {
                    // Select voice
                    data_->selected_voice = buttonID;
                } else {
                    auto step = buttonID - 8;
                    data_->sequencer->ToggleStep(data_->selected_voice, step);
                }
                break;
            case 1:
                //Log::PrintLine("Pressed %d", buttonID + 1);
                break;
            case 2:
                //Log::PrintLine("Long Pressed %d", buttonID + 1);
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
        switch(potID) {
            case 0: {
                auto new_freq = (0.5f + (newPosition * newPosition * 16.f));
                data_->sequencer->internal_clock.SetFreq(new_freq);
                break;
            }
            default: break;
        }
        return false;
    }

    void OnShow() { Log::PrintLine("Page Opened!"); }

    AppData *data_;
};
