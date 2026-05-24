#pragma once
#include <vector>
#include "cells/cell.h"

struct Grid
{
    std::vector<Cell> cells;
    std::vector<Cell> next;
    int rows = 0;
    int cols = 0;

    inline int idx(int i, int j) const { return i * cols + j; }
};

extern Grid grid;