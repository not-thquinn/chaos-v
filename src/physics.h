#pragma once

#ifdef CHAOSV_USE_MPFR
#include <boost/multiprecision/mpfr.hpp>
#endif

#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#ifdef CHAOSV_USE_MPFR
#ifdef CHAOSV_FIXED_MPFR_DIGITS10
using Real = boost::multiprecision::number<
    boost::multiprecision::mpfr_float_backend<
        CHAOSV_FIXED_MPFR_DIGITS10,
        boost::multiprecision::allocate_stack>>;
#else
using Real = boost::multiprecision::mpfr_float;
#endif
#else
using Real = long double;
#endif

void setThreadRealPrecision(unsigned bits);
double toDouble(const Real& value);
Real makeReal(double value);
Real makeReal(const std::string& value);

struct V2 {
    Real x = 0;
    Real y = 0;
};

struct Segment {
    V2 a;
    V2 b;
};

struct Ball {
    V2 p;
    V2 v;
    int id = 0;
    int ballHits = 0;
    std::uint64_t generation = 0;
};

struct Sample {
    Real x;
    Real vx;
    Real lifetime;
    int hits = 0;
    std::vector<int> partnerOffsets;
};

struct AnimFrame {
    std::vector<Ball> balls;
    double duration = 0;
    double start = 0;
};

struct Config {
    double leftDeg = 22;
    double rightDeg = 158;
    std::string leftDegExact;
    std::string rightDegExact;
    double gravity = 400;
    double radius = 8;
    double restitution = .92;
    double gap = 115;
    double segmentLength = 240;
    double spawnX = -57.5;
    double spawnY = -230;
    double spawnInterval = .36;
    double cutoffY = 560;
    int maxBalls = 48;
    int analysisBalls = 96;
    int collisionBudget = 1000;
    int precisionBits = 160;
    bool trackPeriodStability = false;
};

enum class Outcome {
    Unresolved,
    Periodic,
    CollisionBudget,
    SpawnBlocked,
    LiveCapacity
};

struct Result {
    int period = 0;
    int ballsSpawnedAtDetection = 0;
    int collisionsAtDetection = 0;
    bool periodFromRenderedPixel = false;
    double periodStability = std::numeric_limits<double>::quiet_NaN();
    double expansionMargin = std::numeric_limits<double>::quiet_NaN();
    double contractionMargin = std::numeric_limits<double>::quiet_NaN();
    Outcome outcome = Outcome::Unresolved;
    int collisionEvents = 0;
    double radius = 8;
    double gravity = 400;
    double spawnInterval = .36;
    std::vector<int> exitIds;
    std::vector<Sample> exits;
    std::vector<AnimFrame> frames;
    std::vector<Segment> segments;
};

V2 position(const Ball& ball, const Real& time, const Real& gravity);

class Simulator {
public:
    explicit Simulator(const Config& config);
    Result run(bool captureFrames = false, const std::atomic_bool* cancel = nullptr);

private:
    struct Event {
        enum Kind { None, Spawn, Despawn, SegmentHit, Pair } kind = None;
        Real time = 0;
        int a = 0;
        int b = 0;
        std::uint64_t generationA = 0;
        std::uint64_t generationB = 0;
        std::uint64_t order = 0;
    };

    static Config prepare(const Config& config);
    static std::vector<Real> quadraticRoots(
        Real a, Real b, Real c, const Real& low, const Real& high);
    static void copyExits(Result& result, std::vector<std::pair<int, Sample>> raw);

    void advance(std::vector<Ball>& balls, const Real& time) const;
    std::optional<Real> cutoff(const Ball& ball) const;
    std::optional<Real> ballHit(const Ball& first, const Ball& second) const;
    std::optional<Real> segmentHit(const Ball& ball, const Segment& segment) const;
    bool resolvePair(Ball& first, Ball& second) const;
    bool resolveSegment(Ball& ball, const Segment& segment) const;
    std::optional<int> detectStreamingPeriod(
        const std::vector<std::pair<int, Sample>>& raw) const;

    Config config_;
    Real gravity_;
    Real radius_;
    Real restitution_;
    Real spawnInterval_;
    std::vector<Segment> segments_;
};
