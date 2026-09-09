#include "dimenguard/listener/explosion_listener.h"

#include "dimenguard/protection/protection_context.h"

#include <algorithm>
#include <endstone/actor/mob.h>
#include <endstone/block/block.h>
#include <endstone/event/actor/actor_damage_event.h>
#include <endstone/event/actor/actor_explode_event.h>
#include <endstone/event/block/block_explode_event.h>

namespace dimenguard {

ExplosionListener::ExplosionListener(ProtectionContext &context) : context_(context) {}

void ExplosionListener::registerEvents()
{
    context_.registerGuarded(&ExplosionListener::onActorExplosion, *this);
    context_.registerGuarded(&ExplosionListener::onBlockExplosion, *this);
    context_.registerGuarded(&ExplosionListener::onDamage, *this);
}

bool ExplosionListener::allowed(const endstone::Location &origin,
                                const std::vector<endstone::NotNull<endstone::Block>> &blocks)
{
    return context_.allowed(origin, Flag::Explosions) && std::ranges::all_of(blocks, [this](const auto &block) {
               return context_.allowed(block->getLocation(), Flag::Explosions);
           });
}

bool ExplosionListener::onActorExplosion(endstone::ActorExplodeEvent &event)
{
    return allowed(event.getLocation(), event.getBlockList());
}

bool ExplosionListener::onBlockExplosion(endstone::BlockExplodeEvent &event)
{
    return allowed(event.getBlock()->getLocation(), event.getBlockList());
}

bool ExplosionListener::onDamage(endstone::ActorDamageEvent &event)
{
    const auto type = event.getDamageSource()->getType();
    return (type != "block_explosion" && type != "entity_explosion") ||
           context_.allowed(event.getActor()->getLocation(), Flag::Explosions);
}

}
