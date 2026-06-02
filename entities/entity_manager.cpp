#include "entity_manager.h"

#include <algorithm>

void EntityManager::update(SimulationContext& ctx)
{
    for (auto& entity : entities_)
        entity->update(ctx);

    removeDead();
}

void EntityManager::draw() const
{
    for (const auto& entity : entities_)
        entity->draw();
}

void EntityManager::clear()
{
    entities_.clear();
}

void EntityManager::removeDead()
{
    entities_.erase(
        std::remove_if(
            entities_.begin(),
            entities_.end(),
            [](const std::unique_ptr<IEntity>& entity) { return !entity->isAlive(); }),
        entities_.end());
}
