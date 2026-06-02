#pragma once
#include <vector>
#include "../constants.h"
#include "../entities/entity_manager.h"
#include "../grid.h"
#include "../cells/material.h"
#include "../cells/reactions.h"

class World
{
public:
    Grid grid;
    EntityManager entities;

    CellMaterial selectedMaterial = CellMaterial::Sand;
    int radius = 1;

    World(int screenWidth, int screenHeight);

    void update();
    void draw();
    void resizeGrid(int newCols, int newRows);

private:
    bool isEmpty(const Cell& c) const;
    bool inBounds(int i, int j) const;
    bool canMoveTo(int i, int j);
    bool canDisplace(int i, int j, const Cell& cell);
    void moveCell(int fromI, int fromJ, int toI, int toJ, const Cell& cell);
    void displaceCell(int fromI, int fromJ, int toI, int toJ);
    bool tryFallDown(int i, int j, const Cell& cell, bool allowDiagonal);
    bool tryFlowSideways(int i, int j, const Cell& cell);
    bool tryFloat(int i, int j, const Cell& cell, bool allowDiagonal);
    bool tryReact(int i, int j, const Cell& cell);
    void updateLiquid(int i, int j, const Cell& cell);
    void updateGas(int i, int j, const Cell& cell);
    void updateGranular(int i, int j, const Cell& cell);
    void initializeGrid(int screenWidth, int screenHeight);
    void resetGrid();
    void simulateGrid();
    void drawGrid();
    void handleInput();
};
