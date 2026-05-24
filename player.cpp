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

    if (IsKeyDown(KEY_A) && player->posX > 0)              
    { 
        if (grid.cells[player->posY * grid.cols + player->posX - 1].material != CellMaterial::Empty && props(grid.cells[(player->posY - 1) * grid.cols + player->posX - 1].material).blocks)
        {
            if (player->posY > 0 && grid.cells[(player->posY - 1) * grid.cols + player->posX - 1].material == CellMaterial::Empty)
            {
                player->posX--;
                player->posY--;
            }
        }
        else
        {
            player->posX--; 
        }
    }
    if (IsKeyDown(KEY_D) && player->posX < grid.cols - 1)  
    { 
        if (grid.cells[player->posY * grid.cols + player->posX + 1].material != CellMaterial::Empty && props(grid.cells[(player->posY - 1) * grid.cols + player->posX - 1].material).blocks)
        {
            if (player->posY > 0 && grid.cells[(player->posY - 1) * grid.cols + player->posX + 1].material == CellMaterial::Empty)
            {
                player->posX++;
                player->posY--;
            }
        }
        else
        {
            player->posX++; 
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
            if (nextY >= 0 && nextY < grid.rows && grid.cells[grid.idx(nextY, player->posX)].material == CellMaterial::Empty)
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
        if (nextY < grid.rows && grid.cells[grid.idx(nextY, player->posX)].material == CellMaterial::Empty)
            player->grounded = false;
    }
}
