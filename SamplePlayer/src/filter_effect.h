#pragma once
#include <cmath>
#include "daisysp.h"

class FilterEffect {
  public:
    void Init(const float samplerate) {
        filt.Init(samplerate);
        filt.SetDrive(0.1f);
        filt.SetRes(0.6f);
        param    = 0.5f;
        fparam   = 0.5f;
        cur_type = Type::Low;
    }

    /** Takes a 0-1 input and sets the filter cutoff, and mix level
     *  so that from 0-.49 is a LPF, 0.5 is clean, and 0.51 to 1.0 is HPF
     */
    inline void SetCutoffParam(const float in) {
        param = daisysp::fclamp(in, 0.f, 1.f);
    }


    /** Process a single sample through the filter.
     *  The filter mode and dry/wet are determined by the cutoff param.
     */
    float Process(const float in) {
        daisysp::fonepole(fparam, param, 0.005f);
        ComputeCutoff();
        filt.SetFreq(cutoff);
        filt.Process(in);
        auto filtered = cur_type == Type::Low ? filt.Low() : filt.High();
        return (filtered * mix) + (in * (1.f - mix));
    }

  private:
    /** For smooth changes between modes, and settings this is called
     *  within Process at the full sample rate.
     */
    inline void ComputeCutoff() {
        const float maxLpf   = 12000.f;
        const float minLpf   = 100.f;
        const float minHpf   = 100.f;
        const float maxHpf   = 12000.f;
        const float deadzone = 0.04f;
        const float fadezone = 0.10f;

        const float rescale = (fparam * 2.f) - 1.f;
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

    enum class Type { Low, High };
    daisysp::Svf filt;
    float        param, fparam;
    float        cutoff, mix;
    Type         cur_type;
};