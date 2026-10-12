#pragma once
#include "sample_player.h"

/**
 *  @brief Data used throughout the app.
 *
 *  shared context that can store parameters, etc. passed to DSP objects
 *  and accessed from the UI, etc.
 */
struct AppData {
    SampleSequencer *sequencer;

    size_t selected_voice;
};
