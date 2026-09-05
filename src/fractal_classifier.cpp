#include "fractal_classifier.h"

#ifdef CHAOSV_USE_MPFR
#include "fixed_probe.h"
#endif
#include "native_probe.h"

namespace {

#ifdef CHAOSV_USE_MPFR
Result fixedResult(const FixedProbeResult& probe) {
    Result result;
    result.period = probe.period;
    result.ballsSpawnedAtDetection = probe.ballsSpawnedAtDetection;
    result.collisionsAtDetection = probe.collisionsAtDetection;
    result.outcome = probe.outcome;
    result.collisionEvents = probe.collisionEvents;
    result.periodStability = probe.periodStability;
    result.expansionMargin = probe.expansionMargin;
    result.contractionMargin = probe.contractionMargin;
    return result;
}

Result runFixed(Config config, const std::atomic_bool* cancel) {
    if (config.precisionBits <= 128)
        return fixedResult(runFixed128Probe(config, cancel));
    if (config.precisionBits <= 256)
        return fixedResult(runFixed257Probe(config, cancel));
    if (config.precisionBits <= 512)
        return fixedResult(runFixed513Probe(config, cancel));
    return Simulator(config).run(false, cancel);
}
#else
Result runFixed(Config config, const std::atomic_bool* cancel) {
    return Simulator(config).run(false, cancel);
}
#endif

} // namespace

Result classifyFractalPoint(Config config, const std::atomic_bool* cancel) {
    if (config.precisionBits > 64)
        return runFixed(config, cancel);

    Config probeConfig = config;
    // Native arithmetic is IEEE double on this Windows build. The causal
    // detector uses only collision topology, so no floating-point fingerprint
    // matcher or precision-confirmation pass is required here.
    probeConfig.precisionBits = 64;
    const NativeProbeResult probe = runNativeProbe(probeConfig, cancel);
    if (cancel && cancel->load(std::memory_order_relaxed))
        return {};

    Result result;
    result.period = probe.period;
    result.ballsSpawnedAtDetection = probe.ballsSpawnedAtDetection;
    result.collisionsAtDetection = probe.collisionsAtDetection;
    result.outcome = probe.outcome;
    result.collisionEvents = probe.collisionEvents;
    result.periodStability = probe.periodStability;
    result.expansionMargin = probe.expansionMargin;
    result.contractionMargin = probe.contractionMargin;
    return result;
}
