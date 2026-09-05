#pragma once

#include <algorithm>
#include <optional>
#include <vector>

// Finds the smallest collision-closed prefix of the spawn stream. Once every
// ball in that prefix has despawned, no physical state crosses its boundary,
// so the remaining simulation is an exact time-shifted copy of the original.
class CausalPeriodTracker {
public:
    void spawn(int id) {
        if (id != int(parent_.size()))
            return;
        parent_.push_back(id);
        size_.push_back(1);
        minimum_.push_back(id);
        maximum_.push_back(id);
        live_.push_back(true);
        closePrefix();
    }

    void collide(int first, int second) {
        if (first < 0 || second < 0 ||
            first >= int(parent_.size()) || second >= int(parent_.size()))
            return;

        int firstRoot = find(first);
        int secondRoot = find(second);
        if (firstRoot != secondRoot) {
            if (size_[firstRoot] < size_[secondRoot])
                std::swap(firstRoot, secondRoot);
            parent_[secondRoot] = firstRoot;
            size_[firstRoot] += size_[secondRoot];
            minimum_[firstRoot] =
                std::min(minimum_[firstRoot], minimum_[secondRoot]);
            maximum_[firstRoot] =
                std::max(maximum_[firstRoot], maximum_[secondRoot]);
        }

        const int root = find(firstRoot);
        if (minimum_[root] < frontier_)
            frontier_ = std::max(frontier_, maximum_[root] + 1);
        closePrefix();
    }

    void despawn(int id) {
        if (id < 0 || id >= int(live_.size()) || !live_[id])
            return;
        live_[id] = false;
        if (id < accountedFrontier_)
            --livePrefix_;
    }

    std::optional<int> period() const {
        if (accountedFrontier_ == frontier_ && livePrefix_ == 0)
            return frontier_;
        return {};
    }

    int frontier() const { return frontier_; }

private:
    int find(int id) {
        int root = id;
        while (parent_[root] != root)
            root = parent_[root];
        while (parent_[id] != id) {
            const int next = parent_[id];
            parent_[id] = root;
            id = next;
        }
        return root;
    }

    void closePrefix() {
        while (accountedFrontier_ < frontier_ &&
               accountedFrontier_ < int(parent_.size())) {
            const int id = accountedFrontier_++;
            if (live_[id])
                ++livePrefix_;
            const int root = find(id);
            frontier_ = std::max(frontier_, maximum_[root] + 1);
        }
    }

    std::vector<int> parent_;
    std::vector<int> size_;
    std::vector<int> minimum_;
    std::vector<int> maximum_;
    std::vector<bool> live_;
    int frontier_ = 1;
    int accountedFrontier_ = 0;
    int livePrefix_ = 0;
};

