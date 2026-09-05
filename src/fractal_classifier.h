#pragma once

#include "physics.h"

#include <atomic>

// Fast classification used by fractal workers. Precision settings up through
// 64 bits use the native collision-topology engine directly. Higher settings
// select the smallest compiled MPFR precision band that satisfies the request.
Result classifyFractalPoint(
    Config config, const std::atomic_bool* cancel = nullptr);
