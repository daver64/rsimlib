#pragma once

#include <cstdint>

namespace simlib::system {

/** Return elapsed milliseconds from SDL's monotonic timer. */
std::uint64_t time_ms();
/** Delay execution for approximately the requested number of milliseconds. */
void rest(std::uint32_t milliseconds);

} // namespace simlib::system