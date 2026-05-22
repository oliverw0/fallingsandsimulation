#pragma once
#include "grid.h"

struct Player
{
    int posX, posY;
    float velY = 0.0f;
    bool grounded = false;
};

void handlePlayerInput(Player* player);
void drawPlayer(const Player& player);