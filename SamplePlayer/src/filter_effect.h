#pragma once
#include <cmath>
#include "daisysp.h"

class FilterEffect {
  public:
    void Init(const float samplerate) {
        filt.Init(samplerate);
        filt.SetDrive(0.1f);
        filt.SetRes(0.6f);
        mix      = 0.f;
        cur_type = Type::Low;
    }

    /** Takes a 0-1 input and sets the filter cutoff, and mix level
     *  so that from 0-.49 is a LPF, 0.5 is clean, and 0.51 to 1.0 is HPF
     */
    void SetCutoffParam(const float in) {
        const float maxLpf   = 12000.f;
        const float minLpf   = 100.f;
        const float minHpf   = 100.f;
        const float maxHpf   = 12000.f;
        const float deadzone = 0.04f;
        const float fadezone = 0.10f;

        const float clamped = in < 0.f ? 0.f : (in > 1.f ? 1.f : in);
        const float rescale = (clamped * 2.f) - 1.f;
        const float dist    = std::fabs(rescale);

        if(rescale < 0.f) {
            cur_type = Type::Low;
            cutoff   = minLpf * powf(maxLpf / minLpf, 1.f + rescale);
        } else {
            cur_type = Type::High;
            cutoff   = minHpf * powf(maxHpf / minHpf, rescale);
        }

        mix = (dist - deadzone) / fadezone;
        mix = mix < 0.f ? 0.f : (mix > 1.f ? 1.f : mix);
    }

    float Process(const float in) {
        daisysp::fonepole(fcutoff, cutoff, 0.01);
        daisysp::fonepole(fmix, mix, 0.01);
        filt.SetFreq(fcutoff);
        filt.Process(in);
        auto filtered = cur_type == Type::Low ? filt.Low() : filt.High();
        return (filtered * fmix) + (in * (1.f - fmix));
    }

  private:
    enum class Type { Low, High };
    daisysp::Svf filt;
    float        cutoff, mix;
    float        fcutoff, fmix;
    Type         cur_type;
};