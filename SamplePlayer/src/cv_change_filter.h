#pragma once
#include <cmath>

/** Naive hysteresis filter for printing CV input changes
 *
 *  Only update the value if the input has changed by enough to print.
*/
struct CvChangeFilter {
    float prev_val;
    bool  changed;

    /** simple constructor  */
    CvChangeFilter() : prev_val(0.f), changed(false) {}

    /** compares the input to the last value output, and updates it if it exceeds a threshold of 0.1 */
    inline float Process(const float in) {
        changed = false;
        if(std::abs(in - prev_val) > 0.01f) {
            prev_val = in;
            changed  = true;
        }
        return prev_val;
    }

    /** returns true the value changed last time `Process` was called */
    inline bool Changed() const { return changed; }
};