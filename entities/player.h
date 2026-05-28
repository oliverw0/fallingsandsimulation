#pragma once
#include "../grid.h"
#include "entity.h"

class Player : public Entity
{
public:
    Player(int x, int y)
    {
        setPosition(x, y);
    }

    float speed = 1.0f;
    float velY = 0.0f;
    bool grounded = false;
};

void handlePlayerInput(Player* player);
void drawPlayer(const Player& player);