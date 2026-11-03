#pragma once

/** Overdrive distortion effect with a crossfade to the unaffected signal at the bottom portion of the effects range. */
class DistortionEffect {
  public:
    inline void Init() { overdrive.Init(); }

    inline float Process(const float in) {
        daisysp::fonepole(fmix, mix, 0.01f);
        daisysp::fonepole(fdrive, drive, 0.01f);
        daisysp::fonepole(fgain, gain_compensation, 0.01f);
        overdrive.SetDrive(fdrive); //< not free..
        auto out = fmix * overdrive.Process(in) + (in * (1.f - fmix));
        return out * fgain;
    }

    inline void SetDrive(const float amt) {
        drive = 0.25f + (amt * 0.25f);
        mix   = std::min(1.f, amt * 4.f);
        // exponential ~9dB reduction over knob range..
        gain_compensation = 1.f - (amt * amt * 0.66f);
    }

  private:
    daisysp::Overdrive overdrive;

    float mix, drive;
    float fmix, fdrive;
    float gain_compensation, fgain;
};