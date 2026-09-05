#include "fractal_classifier.h"
#include "physics.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    const int side = argc > 1 ? std::max(1, std::atoi(argv[1])) : 24;
    const double xmin = argc > 2 ? std::atof(argv[2]) : 68.;
    const double xmax = argc > 3 ? std::atof(argv[3]) : 74.;
    const double ymin = argc > 4 ? std::atof(argv[4]) : -74.;
    const double ymax = argc > 5 ? std::atof(argv[5]) : -68.;
    const int precisionBits =
        argc > 6 ? std::max(32, std::atoi(argv[6])) : 64;

    Config base;
    base.gravity = 400;
    base.radius = 35;
    base.restitution = .9;
    base.gap = 320;
    base.segmentLength = 150;
    base.spawnX = -160;
    base.spawnY = -230;
    base.spawnInterval = 2;
    base.cutoffY = 560;
    base.maxBalls = 12;
    base.analysisBalls = 50;
    base.collisionBudget = 1000;
    base.precisionBits = precisionBits;
    base.trackPeriodStability = argc > 7 ? std::atoi(argv[7]) != 0 : false;

    int differences = 0;
    int periodic = 0;
    struct HybridValue { Outcome outcome; int period; bool retried; };
    std::vector<HybridValue> hybridResults;
    std::vector<double> stabilityValues;
    hybridResults.reserve(side * side);
    const auto hybridStart = std::chrono::steady_clock::now();
    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            Config point = base;
            point.leftDeg = xmin + (x + .5) / side * (xmax - xmin);
            point.rightDeg = ymax - (y + .5) / side * (ymax - ymin);

            const Result classified = classifyFractalPoint(point);
            const Outcome hybridOutcome = classified.outcome;
            const int hybridPeriod = classified.period;
            if (hybridOutcome == Outcome::Periodic)
                ++periodic;
            hybridResults.push_back({
                hybridOutcome, hybridPeriod,
                false});
            if (std::isfinite(classified.periodStability))
                stabilityValues.push_back(classified.periodStability);
        }
    }
    const double hybridSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - hybridStart).count();

    const auto referenceStart = std::chrono::steady_clock::now();
    int index = 0;
    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            Config point = base;
            point.leftDeg = xmin + (x + .5) / side * (xmax - xmin);
            point.rightDeg = ymax - (y + .5) / side * (ymax - ymin);
            point.precisionBits = precisionBits;
            const Result reference = Simulator(point).run(false);
            if (hybridResults[index].outcome != reference.outcome ||
                hybridResults[index].period != reference.period) {
                std::cerr << "difference x=" << point.leftDeg
                          << " y=" << point.rightDeg
                          << " hybridOutcome=" << int(hybridResults[index].outcome)
                          << " hybridPeriod=" << hybridResults[index].period
                          << " retried=" << hybridResults[index].retried
                          << " referenceOutcome=" << int(reference.outcome)
                          << " referencePeriod=" << reference.period << '\n';
                ++differences;
            }
            ++index;
        }
    }
    const double referenceSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - referenceStart).count();
    std::sort(stabilityValues.begin(), stabilityValues.end());
    const auto percentile = [&](double fraction) {
        return stabilityValues.empty() ? 0. : stabilityValues[
            size_t(fraction * (stabilityValues.size() - 1))];
    };
    std::cout << "simulations=" << side * side
              << " hybrid_seconds=" << hybridSeconds
              << " reference_seconds=" << referenceSeconds
              << " speedup=" << referenceSeconds / hybridSeconds
              << " differences=" << differences
              << " periodic=" << periodic
              << " precision_bits=" << precisionBits << '\n';
    if (!stabilityValues.empty())
        std::cout << "stability_p10=" << percentile(.1)
                  << " p50=" << percentile(.5)
                  << " p90=" << percentile(.9) << '\n';
    return differences == 0 ? 0 : 1;
}
