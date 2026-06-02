#pragma once

#include "simulation_context.h"

class IEntity
{
public:
    virtual ~IEntity() = default;

    virtual void update(SimulationContext& ctx) = 0;
    virtual void draw() const = 0;
    virtual bool isAlive() const { return true; }
};
