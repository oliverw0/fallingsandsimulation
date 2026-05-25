#include <raylib.h>
#include <cstdlib>
#include <vector>
#include <cstring>
#include "player.h"
#include "grid.h"
#include "constants.h"
#include "cells/material.h"

Grid grid;
CellMaterial selectedMaterial = CellMaterial::Sand;
int radius = 1;

static bool isEmpty(const Cell& c)
{
    return c.material == CellMaterial::Empty;
}

static bool inBounds(int i, int j)
{
    return i >= 0 && i < grid.rows && j >= 0 && j < grid.cols;
}

// Destination must be empty in the write buffer (next), not the stale read buffer (cells).
static bool canMoveTo(int i, int j)
{
    if (!inBounds(i, j))
        return false;
    return isEmpty(grid.next[grid.idx(i, j)]);
}

static bool canDisplace(int i, int j, const Cell& cell)
{
    if (!inBounds(i, j))
        return false;
    const Cell& target = grid.cells[grid.idx(i, j)];
    if (isEmpty(target))
        return false;
    return props(target.material).density < props(cell.material).density && !props(target.material).blocks;
}

static void moveCell(int fromI, int fromJ, int toI, int toJ, const Cell& cell)
{
    grid.next[grid.idx(fromI, fromJ)] = Cell{};
    grid.next[grid.idx(toI, toJ)] = cell;
}

static void displaceCell(int fromI, int fromJ, int toI, int toJ)
{
    grid.next[grid.idx(fromI, fromJ)] = grid.cells[grid.idx(toI, toJ)];
    grid.next[grid.idx(toI, toJ)] = grid.cells[grid.idx(fromI, fromJ)];
}

// Returns true if the cell moved.
static bool tryFallDown(int i, int j, const Cell& cell, bool allowDiagonal)
{

    // check allowDiagonal.
    // check diagonal positions.
    // Random chance to fall that way instead of straight down

    if (allowDiagonal) {
        if (canMoveTo(i + 1, j - 1) && GetRandomValue(0, 5) == 1 ? 1 : false) {
            moveCell(i,j,i + 1, j - 1, cell);
            return true;
        }   

        if (canMoveTo(i + 1, j + 1) && GetRandomValue(0, 5) == 1 ? 1 : false) {
            moveCell(i,j,i + 1, j + 1, cell);
            return true;
        }   
    }

    // remainder of logic

    if (canMoveTo(i + 1, j))
    {
        moveCell(i, j, i + 1, j, cell);
        return true;
    }

    if (canDisplace(i + 1, j, cell))
    {
        displaceCell(i, j, i + 1, j);
        return true;
    }

    if (!allowDiagonal)
    {
        return false;
    }

    if (canMoveTo(i + 1, j - 1))
    {
        moveCell(i, j, i + 1, j - 1, cell);
        return true;
    }
    if (canMoveTo(i + 1, j + 1))
    {
        moveCell(i, j, i + 1, j + 1, cell);
        return true;
    }
    return false;
}

static bool tryFlowSideways(int i, int j, const Cell& cell)
{
    int dir = GetRandomValue(0, 1) ? 1 : -1;
    int alt = -dir;

    if (canMoveTo(i, j + dir))
    {
        moveCell(i, j, i, j + dir, cell);
        return true;
    }
    if (canMoveTo(i, j + alt))
    {
        moveCell(i, j, i, j + alt, cell);
        return true;
    }
    return false;
}

static void updateLiquid(int i, int j, const Cell& cell)
{
    if (tryFallDown(i, j, cell, true))
        return;
    if (tryFlowSideways(i, j, cell))
        return;
    // already in next from the frame-start copy
}

static void updateGranular(int i, int j, const Cell& cell)
{
    if (!tryFallDown(i, j, cell, true))
        return; // already in next from the frame-start copy
}

void initializeGrid(int screenWidth, int screenHeight)
{
    grid.rows = screenHeight / cellSize;
    grid.cols = screenWidth / cellSize;
    grid.cells.assign(grid.rows * grid.cols, Cell{});
    grid.next.assign(grid.rows * grid.cols, Cell{});
}

void resizeGrid(int newCols, int newRows)
{
    std::vector<Cell> newCells(newRows * newCols, Cell{});
    std::vector<Cell> newNext(newRows * newCols, Cell{});

    for (int i = 0; i < newRows; i++)
    {
        for (int j = 0; j < newCols; j++)
        {
            if (i < grid.rows && j < grid.cols)
                newCells[i * newCols + j] = grid.cells[grid.idx(i, j)];
        }
    }

    grid.cells.swap(newCells);
    grid.next.swap(newNext);
    grid.cols = newCols;
    grid.rows = newRows;
}

void resetGrid()
{
    std::fill(grid.cells.begin(), grid.cells.end(), Cell{});
    std::fill(grid.next.begin(), grid.next.end(), Cell{});
}

void drawGrid()
{
    // Start each step with a full copy so stone/idle cells survive, then mutate next.
    grid.next = grid.cells;

    // Bottom-up: lower rows update before rows above them.
    for (int i = grid.rows - 1; i >= 0; i--)
    {
        for (int j = 0; j < grid.cols; j++)
        {
            const int k = grid.idx(i, j);
            const Cell& cell = grid.cells[k];

            if (cell.material == CellMaterial::Empty)
                continue;

            // Particle already moved out of this slot earlier this frame.
            if (isEmpty(grid.next[k]))
                continue;

            const MaterialProps& p = props(cell.material);
            if (!p.falls)
                continue;

            if (p.flows)
                updateLiquid(i, j, cell);
            else
                updateGranular(i, j, cell);
        }
    }

    grid.cells.swap(grid.next);

    for (int i = 0; i < grid.rows; i++)
    {
        for (int j = 0; j < grid.cols; j++)
        {
            DrawRectangle(
                j * cellSize, i * cellSize, cellSize, cellSize,
                props(grid.cells[grid.idx(i, j)].material).color
            );
        }
    }
}


void handleInput(int radius)
{
    Vector2 pos = GetMousePosition();
    int x = pos.x / cellSize;
    int y = pos.y / cellSize;

    if (IsKeyPressed(KEY_R))
    {
        resetGrid();
        return;
    }

    if (IsKeyPressed(KEY_ONE)) selectedMaterial = CellMaterial::Sand;
    if (IsKeyPressed(KEY_TWO)) selectedMaterial = CellMaterial::Water;
    if (IsKeyPressed(KEY_THREE)) selectedMaterial = CellMaterial::Stone;

    if (IsKeyPressed(KEY_MINUS) && radius > 0) radius -= 1;
    if (IsKeyPressed(KEY_EQUAL) && radius < 10) radius += 1;

    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            int nx = x + dx;
            int ny = y + dy;

            if (nx >= 0 && nx < grid.cols && ny >= 0 && ny < grid.rows)
            {
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                    grid.cells[grid.idx(ny, nx)] = Cell{ selectedMaterial };
                if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
                    grid.cells[grid.idx(ny, nx)] = Cell{};
            }
        }
    }
}

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1366, 768, "Falling Sand");
    SetTargetFPS(60);

    initializeGrid(GetScreenWidth(), GetScreenHeight());
    Player player = { grid.cols / 2, grid.rows / 2 };

    while (!WindowShouldClose())
    {
        if (IsWindowResized())
            resizeGrid(GetScreenWidth() / cellSize, GetScreenHeight() / cellSize);

        BeginDrawing();
        ClearBackground(BLACK);
        handleInput(radius);
        handlePlayerInput(&player);
        drawGrid();
        drawPlayer(player);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
