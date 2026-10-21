#pragma once

/** Overdrive distortion effect with a crossfade to the unaffected signal at the bottom portion of the effects range. */
class DistortionEffect {
  public:
    inline void Init() { overdrive.Init(); }

    inline float Process(const float in) {
        daisysp::fonepole(fmix, mix, 0.01f);
        daisysp::fonepole(fdrive, drive, 0.01f);
        overdrive.SetDrive(fdrive); //< not free..
        return fmix * overdrive.Process(in) + (in * (1.f - fmix));
    }

    inline void SetDrive(const float amt) {
        drive = 0.25f + (amt * amt * 0.25f);
        mix   = std::min(1.f, amt * 4.f);
    }

  private:
    daisysp::Overdrive overdrive;

    float mix, drive;
    float fmix, fdrive;
};