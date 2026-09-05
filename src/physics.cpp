#include "physics.h"
#include "period_tracker.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <map>
#include <queue>

namespace {

#ifdef CHAOSV_USE_MPFR
constexpr unsigned precisionGuardBits = 8;

constexpr unsigned decimalDigitsToBits(unsigned decimalDigits) {
    return (decimalDigits * 1000u) / 301u +
           ((decimalDigits * 1000u) % 301u ? 2u : 1u);
}

unsigned decimalDigitsForBits(unsigned bits) {
    // Boost's variable MPFR backend specifies precision in decimal digits.
    // Convert the requested binary precision plus a small, explicit guard;
    // the old +8 decimal-digit guard silently added about 27 binary bits.
    const unsigned workingBits = bits + precisionGuardBits;
    unsigned digits = unsigned(std::ceil(workingBits * .3010299956639812));
    while (decimalDigitsToBits(digits) < workingBits)
        ++digits;
    return digits;
}

unsigned realPrecision() {
#ifdef CHAOSV_FIXED_MPFR_DIGITS10
    return std::numeric_limits<Real>::digits;
#else
    return decimalDigitsToBits(Real::thread_default_precision());
#endif
}
#else
unsigned realPrecision() {
    return std::numeric_limits<Real>::digits;
}
#endif

V2 operator+(V2 first, V2 second) {
    return {first.x + second.x, first.y + second.y};
}

V2 operator-(V2 first, V2 second) {
    return {first.x - second.x, first.y - second.y};
}

V2 operator*(V2 vector, const Real& scalar) {
    return {vector.x * scalar, vector.y * scalar};
}

Real dot(V2 first, V2 second) {
    return first.x * second.x + first.y * second.y;
}

Real normSquared(V2 vector) {
    return dot(vector, vector);
}

Real absolute(Real value) {
    return value < 0 ? -value : value;
}

Real epsilon() {
    return ldexp(Real(1), -int(realPrecision() * 2 / 3));
}

// Isolate every real root in [low, high]. Derivative roots split the
// polynomial into monotone intervals, keeping endpoint collision detection
// event-driven rather than sampled.
std::vector<Real> rootsIn(std::vector<Real> coefficients, Real low, Real high) {
    while (coefficients.size() > 1 && coefficients.front() == 0)
        coefficients.erase(coefficients.begin());

    const int degree = int(coefficients.size()) - 1;
    if (degree <= 0 || high < low)
        return {};

    const auto evaluate = [&coefficients](const Real& x) {
        Real value = 0;
        for (const auto& coefficient : coefficients)
            value = value * x + coefficient;
        return value;
    };

    if (degree == 1) {
        const Real root = -coefficients[1] / coefficients[0];
        return root >= low && root <= high ? std::vector<Real>{root}
                                           : std::vector<Real>{};
    }

    std::vector<Real> derivative;
    derivative.reserve(degree);
    for (int i = 0; i < degree; ++i)
        derivative.push_back(coefficients[i] * (degree - i));

    const auto critical = rootsIn(derivative, low, high);
    std::vector<Real> points{low};
    for (const auto& value : critical)
        if (value > low && value < high)
            points.push_back(value);
    points.push_back(high);
    std::sort(points.begin(), points.end());

    std::vector<Real> roots;
    const auto add = [&roots](Real root) {
        for (const auto& existing : roots)
            if (absolute(root - existing) < epsilon() * 16)
                return;
        roots.push_back(root);
    };

    for (const auto& value : critical)
        if (absolute(evaluate(value)) < epsilon() * 128)
            add(value);

    for (size_t i = 0; i + 1 < points.size(); ++i) {
        Real a = points[i];
        Real b = points[i + 1];
        Real fa = evaluate(a);
        Real fb = evaluate(b);

        if (absolute(fa) < epsilon() * 128)
            add(a);
        if (absolute(fb) < epsilon() * 128)
            add(b);
        if ((fa < 0) == (fb < 0))
            continue;

        for (int iteration = 0; iteration < int(realPrecision() * 4) + 20;
             ++iteration) {
            const Real middle = (a + b) / 2;
            const Real fm = evaluate(middle);
            if (absolute(fm) < epsilon() || b - a < epsilon()) {
                a = b = middle;
                break;
            }
            if ((fa < 0) != (fm < 0)) {
                b = middle;
                fb = fm;
            } else {
                a = middle;
                fa = fm;
            }
        }
        add((a + b) / 2);
    }

    std::sort(roots.begin(), roots.end());
    return roots;
}

} // namespace

void setThreadRealPrecision(unsigned bits) {
#ifdef CHAOSV_USE_MPFR
#ifndef CHAOSV_FIXED_MPFR_DIGITS10
    Real::thread_default_precision(decimalDigitsForBits(bits));
#else
    (void)bits;
#endif
#else
    (void)bits;
#endif
}

double toDouble(const Real& value) {
#ifdef CHAOSV_USE_MPFR
    return value.convert_to<double>();
#else
    return static_cast<double>(value);
#endif
}

Real makeReal(double value) {
    return Real(value);
}

Real makeReal(const std::string& value) {
#ifdef CHAOSV_USE_MPFR
    return Real(value);
#else
    return std::strtold(value.c_str(), nullptr);
#endif
}

V2 position(const Ball& ball, const Real& time, const Real& gravity) {
    return {
        ball.p.x + ball.v.x * time,
        ball.p.y + ball.v.y * time + gravity * time * time / 2
    };
}

Simulator::Simulator(const Config& config)
    : config_(prepare(config)),
      gravity_(makeReal(config_.gravity)),
      radius_(makeReal(config_.radius)),
      restitution_(makeReal(std::max(.5, config_.restitution))),
      spawnInterval_(makeReal(config_.spawnInterval)) {
    const Real pi = acos(Real(-1));
    const auto makeSegment =
        [&](double centerX, double degrees, const std::string& exactDegrees) {
        const Real angle = (exactDegrees.empty()
                                ? makeReal(degrees)
                                : makeReal(exactDegrees)) *
                           pi / 180;
        const Real halfLength = makeReal(config_.segmentLength) / 2;
        const V2 direction{cos(angle) * halfLength, sin(angle) * halfLength};
        return Segment{
            {makeReal(centerX) - direction.x, makeReal(80) - direction.y},
            {makeReal(centerX) + direction.x, makeReal(80) + direction.y}
        };
    };

    segments_ = {
        makeSegment(-config_.gap / 2, config_.leftDeg, config_.leftDegExact),
        makeSegment(config_.gap / 2, config_.rightDeg, config_.rightDegExact)
    };
}

Result Simulator::run(bool captureFrames, const std::atomic_bool* cancel) {
    Result result;
    result.segments = segments_;
    result.radius = config_.radius;
    result.gravity = config_.gravity;
    result.spawnInterval = config_.spawnInterval;

    std::vector<Ball> balls;
    std::vector<std::pair<int, Sample>> rawExits;
    std::vector<std::vector<int>> partnerOffsets(1);
    CausalPeriodTracker periodTracker;
    std::optional<int> foundPeriod;
    Real stopAt = 0;

    using PairKey = std::pair<int, int>;
    const auto pairKey = [](int first, int second) -> PairKey {
        return first < second ? PairKey{first, second}
                              : PairKey{second, first};
    };
    std::map<PairKey, double> missMargins;
    std::map<PairKey, double> collisionMargins;
    struct PendingMiss {
        Real time;
        int first = 0;
        int second = 0;
        std::uint64_t firstGeneration = 0;
        std::uint64_t secondGeneration = 0;
        double margin = 0;
    };
    struct LaterMiss {
        bool operator()(const PendingMiss& first, const PendingMiss& second) const {
            return first.time > second.time;
        }
    };
    std::priority_queue<PendingMiss, std::vector<PendingMiss>, LaterMiss>
        pendingMisses;

    // Ball zero is explicit because zero-length events are intentionally
    // ignored by the event selector.
    balls.push_back({{makeReal(config_.spawnX), makeReal(config_.spawnY)},
                     {0, 0}, 0, 0, 0});
    periodTracker.spawn(0);

    Real now = 0;
    int spawned = 1;
    int guard = 0;

    struct LaterEvent {
        bool operator()(const Event& first, const Event& second) const {
            if (first.time != second.time)
                return first.time > second.time;
            const auto priority = [](Event::Kind kind) {
                switch (kind) {
                case Event::Pair: return 0;
                case Event::SegmentHit: return 1;
                case Event::Despawn: return 2;
                case Event::Spawn: return 3;
                case Event::None: return 4;
                }
                return 4;
            };
            if (priority(first.kind) != priority(second.kind))
                return priority(first.kind) > priority(second.kind);
            return first.order > second.order;
        }
    };
    std::priority_queue<Event, std::vector<Event>, LaterEvent> events;
    std::uint64_t eventOrder = 0;

    const auto push = [&](Event event) {
        event.order = eventOrder++;
        events.push(std::move(event));
    };
    const auto findBall = [&](int id) {
        return std::find_if(balls.begin(), balls.end(), [id](const Ball& ball) {
            return ball.id == id;
        });
    };
    const auto recordMiss = [&](int first, int second, double margin) {
        if (!std::isfinite(margin) || margin < 0)
            return;
        const PairKey key = pairKey(first, second);
        const auto existing = missMargins.find(key);
        if (existing == missMargins.end() || margin < existing->second)
            missMargins[key] = margin;
    };
    const auto valid = [&](const Event& event) {
        if (event.kind == Event::Spawn)
            return true;
        const auto first = findBall(event.a);
        if (first == balls.end() || first->generation != event.generationA)
            return false;
        if (event.kind != Event::Pair)
            return true;
        const auto second = findBall(event.b);
        return second != balls.end() &&
               second->generation == event.generationB;
    };
    const auto scheduleBall = [&](const Ball& ball) {
        if (const auto time = cutoff(ball))
            push({Event::Despawn, now + *time, ball.id, 0,
                  ball.generation, 0, 0});
        for (int segment = 0; segment < int(segments_.size()); ++segment) {
            if (const auto time = segmentHit(ball, segments_[segment]))
                push({Event::SegmentHit, now + *time, ball.id, segment,
                      ball.generation, 0, 0});
        }
        for (const auto& other : balls) {
            if (other.id == ball.id)
                continue;
            if (const auto time = ballHit(ball, other)) {
                push({Event::Pair, now + *time, ball.id, other.id,
                      ball.generation, other.generation, 0});
            } else if (config_.trackPeriodStability) {
                const V2 displacement = ball.p - other.p;
                const V2 relativeVelocity = ball.v - other.v;
                const Real a = normSquared(relativeVelocity);
                const Real b = dot(relativeVelocity, displacement);
                const Real c = normSquared(displacement) - 4 * radius_ * radius_;
                if (c <= 0)
                    continue;
                const Real normalization = 4 * radius_ * radius_;
                if (a == 0 || b >= 0) {
                    recordMiss(ball.id, other.id, toDouble(c / normalization));
                    continue;
                }
                const Real discriminant = b * b - a * c;
                const Real tolerance =
                    epsilon() * 128 * (b * b + absolute(a * c) + 1);
                if (discriminant < -tolerance) {
                    const Real closestTime = -b / a;
                    if (closestTime > epsilon() * 8) {
                        pendingMisses.push({
                            now + closestTime, ball.id, other.id,
                            ball.generation, other.generation,
                            toDouble((-discriminant) /
                                     (a * normalization))});
                    }
                }
            }
        }
    };

    const auto computePeriodStability = [&](int period) {
        if (!config_.trackPeriodStability || period <= 0)
            return;

        long double reciprocalSum = 0;
        int expansionCount = 0;
        for (const auto& [key, margin] : missMargins) {
            const bool firstInside = key.first < period;
            const bool secondInside = key.second < period;
            if (firstInside == secondInside)
                continue;
            reciprocalSum += 1.L / std::max<long double>(margin, 1e-30L);
            ++expansionCount;
        }
        if (expansionCount > 0)
            result.expansionMargin =
                double(expansionCount / reciprocalSum);

        if (period == 1) {
            result.contractionMargin =
                std::numeric_limits<double>::infinity();
        } else {
            struct WeightedEdge { int a; int b; double weight; };
            std::vector<WeightedEdge> edges;
            for (const auto& [key, margin] : collisionMargins)
                if (key.first < period && key.second < period)
                    edges.push_back({key.first, key.second, margin});
            std::sort(edges.begin(), edges.end(),
                      [](const auto& first, const auto& second) {
                          return first.weight > second.weight;
                      });
            std::vector<int> parent(period);
            std::vector<int> size(period, 1);
            for (int i = 0; i < period; ++i)
                parent[i] = i;
            const auto root = [&](int node) {
                while (parent[node] != node) {
                    parent[node] = parent[parent[node]];
                    node = parent[node];
                }
                return node;
            };
            int selected = 0;
            double bottleneck = std::numeric_limits<double>::infinity();
            for (const auto& edge : edges) {
                int firstRoot = root(edge.a);
                int secondRoot = root(edge.b);
                if (firstRoot == secondRoot)
                    continue;
                if (size[firstRoot] < size[secondRoot])
                    std::swap(firstRoot, secondRoot);
                parent[secondRoot] = firstRoot;
                size[firstRoot] += size[secondRoot];
                bottleneck = std::min(bottleneck, edge.weight);
                if (++selected == period - 1)
                    break;
            }
            if (selected == period - 1)
                result.contractionMargin = bottleneck;
        }

        const bool hasExpansion = std::isfinite(result.expansionMargin);
        const bool hasContraction = std::isfinite(result.contractionMargin);
        if (hasExpansion && hasContraction) {
            const double first = std::max(result.expansionMargin, 1e-30);
            const double second = std::max(result.contractionMargin, 1e-30);
            result.periodStability = 2 / (1 / first + 1 / second);
        } else if (hasExpansion) {
            result.periodStability = result.expansionMargin;
        } else if (hasContraction) {
            result.periodStability = result.contractionMargin;
        }
    };

    push({Event::Spawn, spawnInterval_ * spawned, 0, 0, 0, 0, 0});
    scheduleBall(balls.front());

    while (int(rawExits.size()) < config_.analysisBalls &&
           ++guard < config_.analysisBalls * 800) {
        if (cancel && cancel->load(std::memory_order_relaxed)) {
            return result;
        }

        while (!events.empty() && !valid(events.top()))
            events.pop();
        if (events.empty())
            break;
        const Event event = events.top();
        events.pop();

        while (!pendingMisses.empty() &&
               pendingMisses.top().time <= event.time) {
            const PendingMiss miss = pendingMisses.top();
            pendingMisses.pop();
            const auto first = findBall(miss.first);
            const auto second = findBall(miss.second);
            if (first != balls.end() && second != balls.end() &&
                first->generation == miss.firstGeneration &&
                second->generation == miss.secondGeneration)
                recordMiss(miss.first, miss.second, miss.margin);
        }

        // Once the period is known, preserve just enough animation to end at
        // the next period boundary.
        if (foundPeriod && event.time >= stopAt) {
            if (captureFrames && result.frames.size() < 2500)
                result.frames.push_back(
                    {balls, toDouble(stopAt - now), toDouble(now)});
            copyExits(result, rawExits);
            result.period = *foundPeriod;
            result.outcome = Outcome::Periodic;
            computePeriodStability(*foundPeriod);
            return result;
        }

        if (captureFrames && result.frames.size() < 2500)
            result.frames.push_back(
                {balls, toDouble(event.time - now), toDouble(now)});

        advance(balls, event.time - now);
        now = event.time;

        switch (event.kind) {
        case Event::Spawn: {
            if (int(balls.size()) >= config_.maxBalls) {
                copyExits(result, rawExits);
                result.outcome = Outcome::LiveCapacity;
                return result;
            }
            const V2 spawn{makeReal(config_.spawnX), makeReal(config_.spawnY)};
            for (const auto& ball : balls) {
                if (normSquared(ball.p - spawn) <= 4 * radius_ * radius_) {
                    result.outcome = Outcome::SpawnBlocked;
                    return result;
                }
            }
            const int id = spawned++;
            balls.push_back({spawn, {0, 0}, id, 0, 0});
            partnerOffsets.emplace_back();
            periodTracker.spawn(id);
            scheduleBall(balls.back());
            push({Event::Spawn, spawnInterval_ * spawned, 0, 0, 0, 0, 0});
            break;
        }
        case Event::Despawn: {
            const auto iterator = findBall(event.a);
            const auto& ball = *iterator;
            const int id = ball.id;
            if (config_.trackPeriodStability) {
                const Real normalization = 4 * radius_ * radius_;
                for (const auto& other : balls) {
                    if (other.id == id)
                        continue;
                    const Real clearance =
                        normSquared(ball.p - other.p) - normalization;
                    if (clearance > 0)
                        recordMiss(id, other.id,
                                   toDouble(clearance / normalization));
                }
            }
            rawExits.push_back({
                id,
                {ball.p.x, ball.v.x, now - spawnInterval_ * ball.id,
                 ball.ballHits, partnerOffsets[id]}
            });
            std::sort(rawExits.back().second.partnerOffsets.begin(),
                      rawExits.back().second.partnerOffsets.end());
            balls.erase(iterator);
            periodTracker.despawn(id);

            if (!foundPeriod) {
                auto period = periodTracker.period();
                if (!period && periodTracker.frontier() > 1)
                    period = detectStreamingPeriod(rawExits);
                if (period) {
                    foundPeriod = *period;
                    result.ballsSpawnedAtDetection = spawned;
                    result.collisionsAtDetection = result.collisionEvents;
                    if (!captureFrames) {
                        copyExits(result, rawExits);
                        result.period = *foundPeriod;
                        result.outcome = Outcome::Periodic;
                        computePeriodStability(*foundPeriod);
                        return result;
                    }
                    const Real cycle = spawnInterval_ * *period;
                    stopAt = (floor(now / cycle) + 1) * cycle;
                }
            }
            break;
        }
        case Event::Pair: {
            auto first = findBall(event.a);
            auto second = findBall(event.b);
            double collisionMargin = 0;
            if (config_.trackPeriodStability) {
                const V2 displacement = first->p - second->p;
                const V2 relativeVelocity = first->v - second->v;
                const Real a = normSquared(relativeVelocity);
                if (a > 0) {
                    const Real b = dot(relativeVelocity, displacement);
                    const Real c = normSquared(displacement) -
                                   4 * radius_ * radius_;
                    Real discriminant = b * b - a * c;
                    if (discriminant < 0)
                        discriminant = 0;
                    collisionMargin = toDouble(
                        discriminant / (a * 4 * radius_ * radius_));
                }
            }
            if (resolvePair(*first, *second)) {
                ++result.collisionEvents;
                partnerOffsets[first->id].push_back(second->id - first->id);
                partnerOffsets[second->id].push_back(first->id - second->id);
                periodTracker.collide(first->id, second->id);
                if (config_.trackPeriodStability) {
                    const PairKey key = pairKey(first->id, second->id);
                    collisionMargins[key] = std::max(
                        collisionMargins[key], collisionMargin);
                }
            }
            ++first->generation;
            ++second->generation;
            const int firstId = first->id;
            const int secondId = second->id;
            scheduleBall(*findBall(firstId));
            scheduleBall(*findBall(secondId));
            break;
        }
        case Event::SegmentHit: {
            auto ball = findBall(event.a);
            if (resolveSegment(*ball, segments_[event.b]))
                ++result.collisionEvents;
            ++ball->generation;
            const int id = ball->id;
            scheduleBall(*findBall(id));
            break;
        }
        case Event::None:
            break;
        }

        if (!foundPeriod && result.collisionEvents > config_.collisionBudget) {
            result.outcome = Outcome::CollisionBudget;
            return result;
        }
    }

    copyExits(result, rawExits);
    return result;
}

Config Simulator::prepare(const Config& config) {
    setThreadRealPrecision(config.precisionBits);
    return config;
}

void Simulator::advance(std::vector<Ball>& balls, const Real& time) const {
    for (auto& ball : balls) {
        ball.p = position(ball, time, gravity_);
        ball.v.y += gravity_ * time;
    }
}

std::vector<Real> Simulator::quadraticRoots(
    Real a, Real b, Real c, const Real& low, const Real& high) {
    const Real scale = std::max(
        Real(1), std::max(absolute(a), std::max(absolute(b), absolute(c))));
    const Real tolerance = epsilon() * 64 * scale;
    std::vector<Real> roots;

    const auto add = [&](Real time) {
        if (time < low || time > high)
            return;
        for (const auto& existing : roots)
            if (absolute(time - existing) <=
                epsilon() * 32 * std::max(Real(1), absolute(time)))
                return;
        roots.push_back(time);
    };

    if (absolute(a) <= tolerance) {
        if (absolute(b) > tolerance)
            add(-c / b);
        std::sort(roots.begin(), roots.end());
        return roots;
    }

    Real discriminant = b * b - 4 * a * c;
    const Real discriminantTolerance =
        epsilon() * 128 * (b * b + absolute(4 * a * c) + 1);
    if (discriminant < -discriminantTolerance)
        return roots;
    if (discriminant < 0)
        discriminant = 0;

    const Real root = sqrt(discriminant);
    if (root == 0) {
        add(-b / (2 * a));
    } else {
        const Real q = -(b + (b < 0 ? -root : root)) / 2;
        add(q / a);
        if (q != 0)
            add(c / q);
    }
    std::sort(roots.begin(), roots.end());
    return roots;
}

std::optional<Real> Simulator::cutoff(const Ball& ball) const {
    const auto roots = quadraticRoots(
        gravity_ / 2, ball.v.y, ball.p.y - makeReal(config_.cutoffY),
        epsilon() * 8, makeReal(10000));
    return roots.empty() ? std::nullopt : std::optional<Real>(roots.front());
}

std::optional<Real> Simulator::ballHit(
    const Ball& first, const Ball& second) const {
    const V2 displacement = first.p - second.p;
    const V2 relativeVelocity = first.v - second.v;
    const Real a = normSquared(relativeVelocity);
    const Real b = dot(relativeVelocity, displacement);
    const Real c = normSquared(displacement) - 4 * radius_ * radius_;
    if (a == 0 || b >= 0 || c <= 0)
        return {};

    Real discriminant = b * b - a * c;
    const Real tolerance =
        epsilon() * 128 * (b * b + absolute(a * c) + 1);
    if (discriminant < -tolerance)
        return {};
    if (discriminant < 0)
        discriminant = 0;

    const Real denominator = -b + sqrt(discriminant);
    if (denominator == 0)
        return {};
    const Real time = c / denominator;
    return time > epsilon() * 8 ? std::optional<Real>(time) : std::nullopt;
}

std::optional<Real> Simulator::segmentHit(
    const Ball& ball, const Segment& segment) const {
    Real horizon = makeReal(10000);
    if (const auto time = cutoff(ball))
        horizon = *time;

    std::vector<Real> candidates;
    const V2 direction = segment.b - segment.a;
    const Real lengthSquared = normSquared(direction);
    const Real length = sqrt(lengthSquared);

    for (const int sign : {-1, 1}) {
        const Real a = gravity_ * direction.x / 2;
        const Real b = direction.x * ball.v.y - direction.y * ball.v.x;
        const Real c = direction.x * (ball.p.y - segment.a.y) -
                       direction.y * (ball.p.x - segment.a.x) -
                       makeReal(sign) * radius_ * length;
        const auto roots = quadraticRoots(a, b, c, epsilon() * 8, horizon);
        for (const auto& time : roots) {
            const V2 point = position(ball, time, gravity_);
            const Real projection = dot(point - segment.a, direction) / lengthSquared;
            if (projection >= 0 && projection <= 1)
                candidates.push_back(time);
        }
    }

    // A segment's endpoints are circles, producing quartic equations.
    for (const V2 endpoint : {segment.a, segment.b}) {
        const V2 offset = ball.p - endpoint;
        std::vector<Real> polynomial = {
            gravity_ * gravity_ / 4,
            gravity_ * ball.v.y,
            normSquared(ball.v) + gravity_ * offset.y,
            2 * dot(ball.v, offset),
            normSquared(offset) - radius_ * radius_
        };
        const auto roots = rootsIn(polynomial, epsilon() * 8, horizon);
        candidates.insert(candidates.end(), roots.begin(), roots.end());
    }

    if (candidates.empty())
        return {};
    std::sort(candidates.begin(), candidates.end());
    return candidates.front();
}

bool Simulator::resolvePair(Ball& first, Ball& second) const {
    V2 normal = second.p - first.p;
    const Real length = sqrt(normSquared(normal));
    if (length == 0)
        return false;
    normal = normal * (1 / length);

    const Real penetration = 2 * radius_ - length;
    if (penetration > 0) {
        const V2 correction = normal * (penetration / 2);
        first.p = first.p - correction;
        second.p = second.p + correction;
    }

    const Real normalVelocity = dot(second.v - first.v, normal);
    if (normalVelocity >= 0)
        return false;

    const Real impulse = -(1 + restitution_) * normalVelocity / 2;
    first.v = first.v - normal * impulse;
    second.v = second.v + normal * impulse;
    ++first.ballHits;
    ++second.ballHits;
    return true;
}

bool Simulator::resolveSegment(Ball& ball, const Segment& segment) const {
    const V2 direction = segment.b - segment.a;
    Real projection = dot(ball.p - segment.a, direction) /
                      normSquared(direction);
    projection = std::max(Real(0), std::min(Real(1), projection));
    const V2 closest = segment.a + direction * projection;
    V2 normal = ball.p - closest;
    const Real length = sqrt(normSquared(normal));
    if (length == 0)
        return false;
    normal = normal * (1 / length);

    const Real penetration = radius_ - length;
    if (penetration > 0)
        ball.p = ball.p + normal * penetration;

    const Real normalVelocity = dot(ball.v, normal);
    if (normalVelocity >= 0)
        return false;
    ball.v = ball.v - normal * ((1 + restitution_) * normalVelocity);
    return true;
}

void Simulator::copyExits(
    Result& result, std::vector<std::pair<int, Sample>> raw) {
    std::sort(raw.begin(), raw.end(), [](const auto& first, const auto& second) {
        return first.first < second.first;
    });
    result.exitIds.clear();
    result.exits.clear();
    for (const auto& [id, sample] : raw) {
        result.exitIds.push_back(id);
        result.exits.push_back(sample);
    }
}

std::optional<int> Simulator::detectStreamingPeriod(
    const std::vector<std::pair<int, Sample>>& raw) const {
    if (raw.size() < 2)
        return {};

    auto ordered = raw;
    std::sort(ordered.begin(), ordered.end(), [](const auto& first, const auto& second) {
        return first.first < second.first;
    });

    const Real tolerance =
        Real(8) * ldexp(Real(1), -std::max(8, config_.precisionBits / 2));
    const auto close = [&](const Real& first, const Real& second) {
        return absolute(first - second) <=
               tolerance * std::max(
                   Real(1), std::max(absolute(first), absolute(second)));
    };
    const auto same = [&](const Sample& first, const Sample& second) {
        return first.hits == second.hits &&
               first.partnerOffsets == second.partnerOffsets &&
               close(first.x, second.x) && close(first.vx, second.vx) &&
               close(first.lifetime, second.lifetime);
    };

    // Examine each consecutive run independently because long-lived balls can
    // despawn out of ID order. Two complete matching suffix cycles, reinforced
    // by a repeating cross-cycle collision edge, are enough evidence here: a
    // third cycle would make short travelling waves unnecessarily expensive.
    for (size_t runStart = 0; runStart < ordered.size();) {
        size_t runEnd = runStart + 1;
        while (runEnd < ordered.size() &&
               ordered[runEnd].first == ordered[runEnd - 1].first + 1)
            ++runEnd;
        const int count = int(runEnd - runStart);

        // Period one is intentionally reserved for causal closure of the
        // independent first ball. Two matching exits in a still-live stream
        // are not enough to prove that the entire stream repeats every ball.
        for (int period = 2; period <= count / 2; ++period) {
            const size_t suffix = runEnd - size_t(2 * period);
            const int firstCycleStart = ordered[suffix].first;
            const int secondCycleStart = firstCycleStart + period;
            bool matches = true;
            bool crossesCycle = false;
            for (int offset = 0; offset < period && matches; ++offset) {
                const int firstId = ordered[suffix + offset].first;
                const int secondId = ordered[suffix + period + offset].first;
                const Sample& first = ordered[suffix + offset].second;
                const Sample& second =
                    ordered[suffix + period + offset].second;
                matches = same(first, second);
                const auto outsideCycle = [period](
                    int id, int partnerOffset, int cycleStart) {
                    const int partner = id + partnerOffset;
                    return partner < cycleStart ||
                           partner >= cycleStart + period;
                };
                for (const int partner : first.partnerOffsets)
                    crossesCycle |= outsideCycle(
                        firstId, partner, firstCycleStart);
                for (const int partner : second.partnerOffsets)
                    crossesCycle |= outsideCycle(
                        secondId, partner, secondCycleStart);
            }
            if (matches && crossesCycle)
                return period;
        }
        runStart = runEnd;
    }
    return {};
}
