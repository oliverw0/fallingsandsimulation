#pragma once
#include "../grid.h"
#include "entity.h"

class Player : public Entity
{
public:
    Player(int x, int y)
    {
        setPosition(x, y);
        setHitbox(1, 3);
    }


    float speed = 1.0f;
    float velY = 0.0f;
    bool grounded = false;
};

void handlePlayerInput(Player* player, Grid& grid);
void drawPlayer(const Player& player);