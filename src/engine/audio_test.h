#pragma once

/** @file
 * @brief Declares the interactive audio test harnesses.
 */

#include "event.h"

namespace audio_test
{

    /** Initialize the interactive audio tests. */
    void initialize();
    /** Handle keyboard input for the interactive audio tests. */
    void handle_event(const sl::Event &event);
    /** Shut down the interactive audio tests. */
    void shutdown();

} // namespace audio_test