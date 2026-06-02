#pragma once

#include "entity.h"
#include "i_entity.h"
#include "simulation_context.h"

class Player : public Entity, public IEntity
{
public:
    Player(int x, int y)
    {
        setPosition(x, y);
        setHitbox(1, 3);
    }

    void update(SimulationContext& ctx) override;
    void draw() const override;

    float speed = 1.0f;
    float velY = 0.0f;
    bool grounded = false;
};
