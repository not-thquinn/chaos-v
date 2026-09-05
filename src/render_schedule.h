#pragma once

#include <vector>

struct RenderTileJob {
    int index = 0;
    bool etaSample = false;
};

std::vector<RenderTileJob> makeTileSchedule(int columns, int rows);
