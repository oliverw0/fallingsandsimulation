#pragma once
#include "player.h"
#include "grid.h"    
#include "constants.h"   
#include <raylib.h>
#include <algorithm>
#include <cmath>

constexpr float GRAVITY     = 0.5f;
constexpr float MAX_FALL    = 8.0f;
constexpr float JUMP_HEIGHT = 20.0f;
constexpr float JUMP_FORCE  = -std::sqrt(2.0f * GRAVITY * JUMP_HEIGHT);

void drawPlayer(const Player& player)
{
    DrawRectangle(player.posX * cellSize, player.posY * cellSize, cellSize, cellSize, RED);
}

void handlePlayerInput(Player* player)
{

    // LEFT movement (A)
    if (IsKeyDown(KEY_A) && player->posX > 0)
    {
        int leftX = player->posX - 1;
        int currY = player->posY;

        // If the cell to the left does not block movement, move
        if (!props(grid.cells[grid.idx(currY, leftX)].material).blocks)
        {
            player->posX--;
        }
        // Else, try to step up and left if not blocked
        else if (player->posY > 0)
        {
            int aboveY = player->posY - 1;
            if (!props(grid.cells[grid.idx(aboveY, leftX)].material).blocks)
            {
                player->posX--;
                player->posY--;
            }
        }
    }

    // RIGHT movement (D)
    if (IsKeyDown(KEY_D) && player->posX < grid.cols - 1)
    {
        int rightX = player->posX + 1;
        int currY = player->posY;

        // If the cell to the right does not block movement, move
        if (!props(grid.cells[grid.idx(currY, rightX)].material).blocks)
        {
            player->posX++;
        }
        // Else, try to step up and right if not blocked
        else if (player->posY > 0)
        {
            int aboveY = player->posY - 1;
            if (!props(grid.cells[grid.idx(aboveY, rightX)].material).blocks)
            {
                player->posX++;
                player->posY--;
            }
        }
    }

    if ((IsKeyPressed(KEY_W) || IsKeyPressed(KEY_SPACE)) && player->grounded)
    {
        player->velY = JUMP_FORCE;
        player->grounded = false;
    }

    if (!player->grounded)
    {
        player->velY = std::min(player->velY + GRAVITY, MAX_FALL);

        int steps = (int)std::abs(player->velY);
        int dir   = player->velY >= 0 ? 1 : -1;

        for (int s = 0; s < steps; s++)
        {
            int nextY = player->posY + dir;
            if (nextY >= 0 && nextY < grid.rows && !props(grid.cells[grid.idx(nextY, player->posX)].material).blocks)
            {
                player->posY = nextY;
                player->grounded = false;
            }
            else
            {
                player->velY = 0.0f;
                if (dir == 1) player->grounded = true;
                break;
            }
        }
    }
    else
    {
        // if sand beneath was erased, start falling
        int nextY = player->posY + 1;
        if (nextY < grid.rows && !props(grid.cells[grid.idx(nextY, player->posX)].material).blocks)
   
            player->grounded = false;
    }
}
