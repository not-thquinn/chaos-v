#include "period_tracker.h"

#include <iostream>

namespace {

bool expect(const char* name, const CausalPeriodTracker& tracker, int period) {
    const auto detected = tracker.period();
    if (detected && *detected == period)
        return true;
    std::cerr << name << " failed: expected=" << period << " actual="
              << (detected ? *detected : 0) << '\n';
    return false;
}

} // namespace

int main() {
    bool passed = true;

    CausalPeriodTracker independent;
    independent.spawn(0);
    independent.despawn(0);
    passed &= expect("independent ball", independent, 1);

    CausalPeriodTracker pair;
    pair.spawn(0);
    pair.spawn(1);
    pair.collide(0, 1);
    pair.despawn(0);
    if (pair.period()) {
        std::cerr << "live pair closed prematurely\n";
        passed = false;
    }
    pair.despawn(1);
    passed &= expect("contiguous pair", pair, 2);

    CausalPeriodTracker interleaved;
    for (int id = 0; id < 4; ++id)
        interleaved.spawn(id);
    interleaved.collide(0, 2);
    interleaved.collide(1, 3);
    for (int id = 0; id < 4; ++id)
        interleaved.despawn(id);
    passed &= expect("interleaved components", interleaved, 4);

    CausalPeriodTracker retroactive;
    for (int id = 0; id < 5; ++id)
        retroactive.spawn(id);
    retroactive.collide(2, 4);
    retroactive.collide(0, 2);
    for (int id = 0; id < 5; ++id)
        retroactive.despawn(id);
    passed &= expect("retroactive closure", retroactive, 5);

    return passed ? 0 : 1;
}
