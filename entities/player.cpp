#include "player.h"

#include "../constants.h"
#include "../cells/material.h"
#include <raylib.h>
#include <algorithm>
#include <cmath>

constexpr float GRAVITY     = 0.5f;
constexpr float MAX_FALL    = 8.0f;
constexpr float JUMP_HEIGHT = 20.0f;
constexpr float JUMP_FORCE  = -std::sqrt(2.0f * GRAVITY * JUMP_HEIGHT);

void Player::draw() const
{
    DrawRectangle(getPosX() * cellSize, getPosY() * cellSize, cellSize, cellSize, RED);
}

void Player::update(SimulationContext& ctx)
{
    Grid& grid = ctx.grid;
    int posX = getPosX();
    int posY = getPosY();

    if (IsKeyDown(KEY_A) && posX > 0)
    {
        int leftX = posX - 1;
        int currY = posY;

        if (!props(grid.cells[grid.idx(currY, leftX)].material).blocks)
            setPosX(leftX);
        else if (posY > 0)
        {
            int aboveY = posY - 1;
            if (!props(grid.cells[grid.idx(aboveY, leftX)].material).blocks)
                setPosition(leftX, aboveY);
        }
    }

    if (IsKeyDown(KEY_D) && posX < grid.cols - 1)
    {
        int rightX = posX + 1;
        int currY = posY;

        if (!props(grid.cells[grid.idx(currY, rightX)].material).blocks)
            setPosX(rightX);
        else if (posY > 0)
        {
            int aboveY = posY - 1;
            if (!props(grid.cells[grid.idx(aboveY, rightX)].material).blocks)
                setPosition(rightX, aboveY);
        }
    }

    if ((IsKeyPressed(KEY_W) || IsKeyPressed(KEY_SPACE)) && grounded)
    {
        velY = JUMP_FORCE;
        grounded = false;
    }

    if (!grounded)
    {
        velY = std::min(velY + GRAVITY, MAX_FALL);

        int steps = (int)std::abs(velY);
        int dir   = velY >= 0 ? 1 : -1;

        for (int s = 0; s < steps; s++)
        {
            int nextY = getPosY() + dir;
            if (nextY >= 0 && nextY < grid.rows && !props(grid.cells[grid.idx(nextY, getPosX())].material).blocks)
            {
                setPosY(nextY);
                grounded = false;
            }
            else
            {
                velY = 0.0f;
                if (dir == 1) grounded = true;
                break;
            }
        }
    }
    else
    {
        int nextY = getPosY() + 1;
        if (nextY < grid.rows && !props(grid.cells[grid.idx(nextY, getPosX())].material).blocks)
            grounded = false;
    }
}
