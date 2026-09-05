#include "render_schedule.h"

#include <algorithm>
#include <numeric>
#include <random>

std::vector<RenderTileJob> makeTileSchedule(int columns, int rows) {
    const int count = columns * rows;
    std::vector<int> randomOrder(count);
    std::iota(randomOrder.begin(), randomOrder.end(), 0);
    std::mt19937 generator(std::random_device{}());
    std::shuffle(randomOrder.begin(), randomOrder.end(), generator);

    std::vector<bool> queued(count, false);
    std::vector<RenderTileJob> schedule;
    schedule.reserve(count);
    int readingCursor = 0;
    int randomCursor = 0;

    const auto appendReading = [&] {
        while (readingCursor < count && queued[readingCursor])
            ++readingCursor;
        if (readingCursor >= count)
            return;
        queued[readingCursor] = true;
        schedule.push_back({readingCursor++, false});
    };

    const auto appendRandom = [&] {
        while (randomCursor < count && queued[randomOrder[randomCursor]])
            ++randomCursor;
        if (randomCursor >= count)
            return;
        const int index = randomOrder[randomCursor++];
        queued[index] = true;
        schedule.push_back({index, true});
    };

    while (int(schedule.size()) < count) {
        appendReading();
        appendRandom();
    }
    return schedule;
}
