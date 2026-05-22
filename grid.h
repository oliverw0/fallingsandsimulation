#pragma once
#include <vector>

struct Grid
{
    std::vector<int> cells;
    std::vector<int> next;
    int rows = 0;
    int cols = 0;

    inline int idx(int i, int j) const { return i * cols + j; }
};

extern Grid grid;