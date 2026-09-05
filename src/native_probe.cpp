#include "native_probe.h"

NativeProbeResult runNativeProbe(
    const Config& config, const std::atomic_bool* cancel) {
    const auto result = Simulator(config).run(false, cancel);
    return {
        result.period,
        result.ballsSpawnedAtDetection,
        result.collisionsAtDetection,
        result.outcome,
        result.collisionEvents,
        result.periodStability,
        result.expansionMargin,
        result.contractionMargin
    };
}
