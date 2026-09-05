#include "physics.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>

int main(int argc, char** argv) {
    const int side = argc > 1 ? std::max(1, std::atoi(argv[1])) : 24;
    const double xmin = argc > 2 ? std::atof(argv[2]) : 68.;
    const double xmax = argc > 3 ? std::atof(argv[3]) : 74.;
    const double ymin = argc > 4 ? std::atof(argv[4]) : -74.;
    const double ymax = argc > 5 ? std::atof(argv[5]) : -68.;

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
    base.precisionBits = 64;

    struct PeriodStats {
        int simulations = 0;
        long long spawned = 0;
    };
    std::map<int, PeriodStats> byPeriod;
    int periodic = 0;
    long long spawned = 0;

    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            Config point = base;
            point.leftDeg = xmin + (x + .5) / side * (xmax - xmin);
            point.rightDeg = ymax - (y + .5) / side * (ymax - ymin);
            const Result result = Simulator(point).run(false);
            if (result.outcome != Outcome::Periodic)
                continue;
            ++periodic;
            spawned += result.ballsSpawnedAtDetection;
            auto& stats = byPeriod[result.period];
            ++stats.simulations;
            stats.spawned += result.ballsSpawnedAtDetection;
        }
    }

    std::cout << "periodic=" << periodic
              << " total_spawned=" << spawned
              << " average_spawned="
              << (periodic ? double(spawned) / periodic : 0.) << '\n';
    for (const auto& [period, stats] : byPeriod)
        std::cout << "period=" << period
                  << " simulations=" << stats.simulations
                  << " average_spawned="
                  << double(stats.spawned) / stats.simulations << '\n';
}
