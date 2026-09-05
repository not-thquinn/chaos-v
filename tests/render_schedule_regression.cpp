#include "render_schedule.h"

#include <iostream>
#include <vector>

int main() {
    for (const auto [columns, rows] : {
             std::pair{1, 1}, std::pair{2, 7}, std::pair{64, 64},
             std::pair{17, 31}}) {
        const int count = columns * rows;
        const auto schedule = makeTileSchedule(columns, rows);
        if (int(schedule.size()) != count) {
            std::cerr << "wrong schedule size\n";
            return 1;
        }

        std::vector<int> occurrences(count, 0);
        int samples = 0;
        for (const auto& job : schedule) {
            if (job.index < 0 || job.index >= count) {
                std::cerr << "out-of-range tile\n";
                return 1;
            }
            ++occurrences[job.index];
            samples += job.etaSample;
        }
        for (const int occurrence : occurrences) {
            if (occurrence != 1) {
                std::cerr << "duplicate or missing tile\n";
                return 1;
            }
        }
        if (count > 1 && samples == 0) {
            std::cerr << "schedule has no random ETA samples\n";
            return 1;
        }
    }
    return 0;
}
