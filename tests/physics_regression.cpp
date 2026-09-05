#include "physics.h"

#include <atomic>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Expected {
    std::string name;
    Config config;
    Outcome outcome;
    int period;
    int spawnedAtDetection;
    int collisionsAtDetection = -1;
};

Config common(double left, double right) {
    Config config;
    config.leftDeg = left;
    config.rightDeg = right;
    config.gravity = 400;
    config.radius = 35;
    config.restitution = .9;
    config.gap = 320;
    config.segmentLength = 150;
    config.spawnX = -160;
    config.spawnY = -230;
    config.spawnInterval = 2;
    config.cutoffY = 560;
    config.maxBalls = 12;
    config.analysisBalls = 50;
    config.collisionBudget = 1000;
    config.precisionBits = 64;
    return config;
}

bool check(const Expected& expected) {
    const Result result = Simulator(expected.config).run(false);
    const bool passed =
        result.outcome == expected.outcome &&
        result.period == expected.period &&
        (expected.spawnedAtDetection < 0 ||
         result.ballsSpawnedAtDetection == expected.spawnedAtDetection) &&
        (expected.collisionsAtDetection < 0 ||
         result.collisionsAtDetection == expected.collisionsAtDetection);
    if (!passed) {
        std::cerr << expected.name << " failed: outcome=" << int(result.outcome)
                  << " period=" << result.period
                  << " spawned=" << result.ballsSpawnedAtDetection
                  << " collisionsAtDetection="
                  << result.collisionsAtDetection << '\n';
    }
    return passed;
}

} // namespace

int main() {
    std::vector<Expected> cases;
    cases.push_back({
        "period 3", common(72.0703125, -70.6640625),
        Outcome::Periodic, 3, 5
    });
    cases.push_back({
        "period 6 broad", common(41.120895359374998, -71.947166175781248),
        Outcome::Periodic, 6, 8
    });
    cases.push_back({
        "period 6 precision confirmation",
        common(71.3525576171875, -71.230935058593744),
        Outcome::Periodic, 6, 7
    });
    cases.push_back({
        "period 5 precision confirmation",
        common(71.331697588867186, -71.248290801757818),
        Outcome::Periodic, 5, 7
    });
    cases.push_back({
        "period 2 travelling stream",
        common(74.71976834763268, -41.282606356111067),
        Outcome::Periodic, 2, 9, 28
    });

    Config reportedTwentyTwo =
        common(36.5950033242447, -48.08591010396825);
    reportedTwentyTwo.leftDegExact =
        "36.5950033242447035245240265217831821235";
    reportedTwentyTwo.rightDegExact =
        "-48.0859101039682542435295869007315153084";
    reportedTwentyTwo.spawnInterval = 1;
    cases.push_back({
        "reported false period one", reportedTwentyTwo,
        Outcome::Periodic, 22, 25, 100
    });

    Config reportedFour =
        common(33.77798408629245, -37.80149595282757);
    reportedFour.leftDegExact =
        "33.7779840862924485319729480493937082820";
    reportedFour.rightDegExact =
        "-37.8014959528275746712983768189985373964";
    cases.push_back({
        "reported unresolved period four", reportedFour,
        Outcome::Periodic, 4, 12, 48
    });

    Config periodOne = common(45, -45);
    periodOne.spawnInterval = 10;
    periodOne.analysisBalls = 8;
    cases.push_back({"period 1", periodOne, Outcome::Periodic, 1, -1});

    Config capacity = common(72.0703125, -70.6640625);
    capacity.maxBalls = 1;
    cases.push_back({"live capacity", capacity, Outcome::LiveCapacity, 0, 0});

    Config blocked = common(72.0703125, -70.6640625);
    blocked.spawnInterval = .01;
    cases.push_back({"spawn blocked", blocked, Outcome::SpawnBlocked, 0, 0});

    Config budget = common(72.0703125, -70.6640625);
    budget.collisionBudget = 1;
    cases.push_back({"collision budget", budget, Outcome::CollisionBudget, 0, 0});

    bool passed = true;
    for (const auto& test : cases)
        passed &= check(test);

    const Config deterministicConfig = common(72.0703125, -70.6640625);
    const Result first = Simulator(deterministicConfig).run(false);
    const Result second = Simulator(deterministicConfig).run(false);
    if (first.outcome != second.outcome || first.period != second.period ||
        first.collisionEvents != second.collisionEvents ||
        first.collisionsAtDetection != second.collisionsAtDetection ||
        first.ballsSpawnedAtDetection != second.ballsSpawnedAtDetection ||
        first.exitIds != second.exitIds) {
        std::cerr << "deterministic repeat failed\n";
        passed = false;
    }

    if (first.outcome == Outcome::Periodic &&
        first.collisionsAtDetection != first.collisionEvents) {
        std::cerr << "classification continued after causal closure\n";
        passed = false;
    }

    const Result animated = Simulator(deterministicConfig).run(true);
    if (animated.outcome != Outcome::Periodic || animated.period != 3 ||
        animated.collisionsAtDetection != first.collisionsAtDetection ||
        animated.collisionEvents < animated.collisionsAtDetection ||
        animated.frames.empty()) {
        std::cerr << "animated causal-period capture failed\n";
        passed = false;
    }

    const Config streamConfig =
        common(74.71976834763268, -41.282606356111067);
    const Result animatedStream = Simulator(streamConfig).run(true);
    if (animatedStream.outcome != Outcome::Periodic ||
        animatedStream.period != 2 ||
        animatedStream.ballsSpawnedAtDetection != 9 ||
        animatedStream.collisionsAtDetection != 28 ||
        animatedStream.frames.empty()) {
        std::cerr << "animated travelling-period capture failed\n";
        passed = false;
    }

    const Result animatedTwentyTwo = Simulator(reportedTwentyTwo).run(true);
    const Result animatedFour = Simulator(reportedFour).run(true);
    if (animatedTwentyTwo.outcome != Outcome::Periodic ||
        animatedTwentyTwo.period != 22 ||
        animatedFour.outcome != Outcome::Periodic || animatedFour.period != 4) {
        std::cerr << "reported-period animation capture failed\n";
        passed = false;
    }

    std::atomic_bool cancelled{true};
    const Result cancelledResult =
        Simulator(deterministicConfig).run(false, &cancelled);
    if (cancelledResult.outcome != Outcome::Unresolved ||
        cancelledResult.collisionEvents != 0) {
        std::cerr << "cancellation failed\n";
        passed = false;
    }

    if (passed)
        std::cout << "All physics regressions passed\n";
    return passed ? 0 : 1;
}
