#include "player.h"
#include "../grid.h"    
#include "../constants.h"   
#include <raylib.h>
#include <algorithm>
#include <cmath>

constexpr float GRAVITY     = 0.5f;
constexpr float MAX_FALL    = 8.0f;
constexpr float JUMP_HEIGHT = 20.0f;
constexpr float JUMP_FORCE  = -std::sqrt(2.0f * GRAVITY * JUMP_HEIGHT);

void drawPlayer(const Player& player)
{
    DrawRectangle(player.getPosition()[0] * cellSize, player.getPosition()[1] * cellSize, cellSize, cellSize, RED);
}

void handlePlayerInput(Player* player)
{
    int posX = player->getPosX();
    int posY = player->getPosY();

    // LEFT movement (A)
    if (IsKeyDown(KEY_A) && posX > 0)
    {
        int leftX = posX - 1;
        int currY = posY;

        // If the cell to the left does not block movement, move
        if (!props(grid.cells[grid.idx(currY, leftX)].material).blocks)
        {
            player->setPosX(leftX);
        }
        // Else, try to step up and left if not blocked
        else if (posY > 0)
        {
            int aboveY = posY - 1;
            if (!props(grid.cells[grid.idx(aboveY, leftX)].material).blocks)
            {
                player->setPosition(leftX, aboveY);
            }
        }
    }

    // RIGHT movement (D)
    if (IsKeyDown(KEY_D) && posX < grid.cols - 1)
    {
        int rightX = posX + 1;
        int currY = posY;

        // If the cell to the right does not block movement, move
        if (!props(grid.cells[grid.idx(currY, rightX)].material).blocks)
        {
            player->setPosX(rightX);
        }
        // Else, try to step up and right if not blocked
        else if (posY > 0)
        {
            int aboveY = posY - 1;
            if (!props(grid.cells[grid.idx(aboveY, rightX)].material).blocks)
            {
                player->setPosition(rightX, aboveY);
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
            int nextY = player->getPosY() + dir;
            if (nextY >= 0 && nextY < grid.rows && !props(grid.cells[grid.idx(nextY, player->getPosX())].material).blocks)
            {
                player->setPosY(nextY);
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
        int nextY = player->getPosY() + 1;
        if (nextY < grid.rows && !props(grid.cells[grid.idx(nextY, player->getPosX())].material).blocks)
        {
            player->grounded = false;
        }
    }
}
