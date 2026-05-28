#include "world.h"

#include <algorithm>
#include <raylib.h>

World::World(int screenWidth, int screenHeight)
    : player(0, 0)
{
    initializeGrid(screenWidth, screenHeight);
    player.setPosition(grid.cols / 2, grid.rows / 2);
}

bool World::isEmpty(const Cell& c) const
{
    return c.material == CellMaterial::Empty;
}

bool World::inBounds(int i, int j) const
{
    return i >= 0 && i < grid.rows && j >= 0 && j < grid.cols;
}

bool World::canMoveTo(int i, int j)
{
    if (!inBounds(i, j))
        return false;
    return isEmpty(grid.next[grid.idx(i, j)]);
}

bool World::canDisplace(int i, int j, const Cell& cell)
{
    if (!inBounds(i, j))
        return false;
    const Cell& target = grid.cells[grid.idx(i, j)];
    if (isEmpty(target))
        return false;
    return props(target.material).density < props(cell.material).density && !props(target.material).blocks;
}

void World::moveCell(int fromI, int fromJ, int toI, int toJ, const Cell& cell)
{
    grid.next[grid.idx(fromI, fromJ)] = Cell{};
    grid.next[grid.idx(toI, toJ)] = cell;
}

void World::displaceCell(int fromI, int fromJ, int toI, int toJ)
{
    grid.next[grid.idx(fromI, fromJ)] = grid.cells[grid.idx(toI, toJ)];
    grid.next[grid.idx(toI, toJ)] = grid.cells[grid.idx(fromI, fromJ)];
}

bool World::tryFallDown(int i, int j, const Cell& cell, bool allowDiagonal)
{
    if (allowDiagonal) {
        if (canMoveTo(i + 1, j - 1) && GetRandomValue(0, 5) == 1) {
            moveCell(i, j, i + 1, j - 1, cell);
            return true;
        }

        if (canMoveTo(i + 1, j + 1) && GetRandomValue(0, 5) == 1) {
            moveCell(i, j, i + 1, j + 1, cell);
            return true;
        }
    }

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
        return false;

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

bool World::tryFlowSideways(int i, int j, const Cell& cell)
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

bool World::tryFloat(int i, int j, const Cell& cell, bool allowDiagonal)
{
    if (allowDiagonal) {
        if (canMoveTo(i - 1, j - 1) && GetRandomValue(0, 5) == 1) {
            moveCell(i, j, i - 1, j - 1, cell);
            return true;
        }

        if (canMoveTo(i - 1, j + 1) && GetRandomValue(0, 5) == 1) {
            moveCell(i, j, i - 1, j + 1, cell);
            return true;
        }
    }

    if (canMoveTo(i - 1, j))
    {
        moveCell(i, j, i - 1, j, cell);
        return true;
    }

    if (canDisplace(i - 1, j, cell))
    {
        displaceCell(i, j, i - 1, j);
        return true;
    }

    if (!allowDiagonal)
        return false;

    if (canMoveTo(i - 1, j - 1))
    {
        moveCell(i, j, i - 1, j - 1, cell);
        return true;
    }
    if (canMoveTo(i - 1, j + 1))
    {
        moveCell(i, j, i - 1, j + 1, cell);
        return true;
    }
    return false;
}

bool World::tryReact(int i, int j, const Cell& cell)
{
    const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};

    for (auto& d : dirs)
    {
        int ni = i + d[0], nj = j + d[1];
        if (!inBounds(ni, nj))
            continue;

        const Cell& neighbour = grid.cells[grid.idx(ni, nj)];

        if (neighbour.material == CellMaterial::Empty)
            continue;

        const Reaction* r = findReaction(cell.material, neighbour.material);

        if (!r)
            continue;

        if (r->chance > 1 && GetRandomValue(0, r->chance - 1) != 0)
            continue;

        grid.next[grid.idx(i, j)] = Cell{ r->resultA };
        grid.next[grid.idx(ni, nj)] = Cell{ r->resultB };
        return true;
    }

    return false;
}

void World::updateLiquid(int i, int j, const Cell& cell)
{
    if (tryFallDown(i, j, cell, true))
        return;
    if (tryFlowSideways(i, j, cell))
        return;
}

void World::updateGas(int i, int j, const Cell& cell)
{
    if (tryFloat(i, j, cell, true))
        return;
    if (tryFlowSideways(i, j, cell))
        return;
}

void World::updateGranular(int i, int j, const Cell& cell)
{
    if (!tryFallDown(i, j, cell, true))
        return;
}

void World::initializeGrid(int screenWidth, int screenHeight)
{
    grid.rows = screenHeight / cellSize;
    grid.cols = screenWidth / cellSize;
    grid.cells.assign(grid.rows * grid.cols, Cell{});
    grid.next.assign(grid.rows * grid.cols, Cell{});
}

void World::resizeGrid(int newCols, int newRows)
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

void World::resetGrid()
{
    std::fill(grid.cells.begin(), grid.cells.end(), Cell{});
    std::fill(grid.next.begin(), grid.next.end(), Cell{});
}

void World::simulateGrid()
{
    grid.next = grid.cells;

    for (int i = grid.rows - 1; i >= 0; i--)
    {
        for (int j = 0; j < grid.cols; j++)
        {
            const int k = grid.idx(i, j);
            const Cell& cell = grid.cells[k];

            if (cell.material == CellMaterial::Empty)
                continue;

            if (isEmpty(grid.next[k]))
                continue;

            const MaterialProps& p = props(cell.material);

            if (!p.falls)
                continue;

            if (p.density < 0)
            {
                if (!tryReact(i, j, cell))
                    updateGas(i, j, cell);
            }
            else if (p.flows)
            {
                if (!tryReact(i, j, cell))
                    updateLiquid(i, j, cell);
            }
            else
            {
                if (!tryReact(i, j, cell))
                    updateGranular(i, j, cell);
            }
        }
    }

    grid.cells.swap(grid.next);

}

void World::drawGrid()
{
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

void World::handleInput()
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
    if (IsKeyPressed(KEY_FOUR)) selectedMaterial = CellMaterial::Lava;
    if (IsKeyPressed(KEY_FIVE)) selectedMaterial = CellMaterial::Steam;

    if (IsKeyPressed(KEY_MINUS) && radius > 0) radius -= 1;
    if (IsKeyPressed(KEY_EQUAL) && radius < 10) radius += 1;

    int bx = radius, by = 0, berr = 1 - radius;
    while (bx >= by)
    {
        for (int scanX = x - bx; scanX <= x + bx; ++scanX)
        {
            if (scanX >= 0 && scanX < grid.cols)
            {
                if (y + by >= 0 && y + by < grid.rows)
                {
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                        grid.cells[grid.idx(y + by, scanX)] = Cell{ selectedMaterial };
                    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
                        grid.cells[grid.idx(y + by, scanX)] = Cell{};
                }
                if (y - by >= 0 && y - by < grid.rows)
                {
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                        grid.cells[grid.idx(y - by, scanX)] = Cell{ selectedMaterial };
                    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
                        grid.cells[grid.idx(y - by, scanX)] = Cell{};
                }
            }
        }
        for (int scanX = x - by; scanX <= x + by; ++scanX)
        {
            if (scanX >= 0 && scanX < grid.cols)
            {
                if (y + bx >= 0 && y + bx < grid.rows)
                {
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                        grid.cells[grid.idx(y + bx, scanX)] = Cell{ selectedMaterial };
                    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
                        grid.cells[grid.idx(y + bx, scanX)] = Cell{};
                }
                if (y - bx >= 0 && y - bx < grid.rows)
                {
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                        grid.cells[grid.idx(y - bx, scanX)] = Cell{ selectedMaterial };
                    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
                        grid.cells[grid.idx(y - bx, scanX)] = Cell{};
                }
            }
        }

        ++by;
        if (berr < 0)
            berr += 2 * by + 1;
        else
        {
            --bx;
            berr += 2 * (by - bx) + 1;
        }
    }
}

void World::update()
{
    if (IsWindowResized())
        resizeGrid(GetScreenWidth() / cellSize, GetScreenHeight() / cellSize);

    simulateGrid();
    handleInput();
    handlePlayerInput(&player, grid);
}

void World::draw()
{
    BeginDrawing();
    ClearBackground(BLACK);
    drawGrid();
    drawPlayer(player);
    EndDrawing();
}
