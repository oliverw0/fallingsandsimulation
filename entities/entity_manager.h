#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "i_entity.h"
#include "simulation_context.h"

class EntityManager
{
public:
    template<typename T, typename... Args>
    T& spawn(Args&&... args)
    {
        auto entity = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *entity;
        entities_.push_back(std::move(entity));
        return ref;
    }

    void update(SimulationContext& ctx);
    void draw() const;
    void clear();

private:
    void removeDead();

    std::vector<std::unique_ptr<IEntity>> entities_;
};
