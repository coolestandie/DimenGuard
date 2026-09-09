#include "dimenguard/listener/explosion_listener.h"

#include "dimenguard/protection/protection_context.h"

#include <endstone/actor/actor.h>
#include <endstone/actor/mob.h>
#include <endstone/block/block.h>
#include <endstone/event/actor/actor_damage_event.h>
#include <endstone/event/actor/actor_explode_event.h>
#include <endstone/event/block/block_explode_event.h>

namespace dimenguard {
namespace {

std::optional<ExplosionActorType> actorType(const endstone::Nullable<endstone::Actor> &actor)
{
    if (!actor) {
        return std::nullopt;
    }
    const auto id = actor->getType().getId();
    return ExplosionActorType{id.getNamespace(), id.getKey()};
}

}

ExplosionListener::ExplosionListener(ProtectionContext &context) : context_(context) {}

void ExplosionListener::registerEvents()
{
    context_.registerGuarded(&ExplosionListener::onActorExplosion, *this);
    context_.registerGuarded(&ExplosionListener::onBlockExplosion, *this);
    context_.registerGuarded(&ExplosionListener::onDamage, *this);
}

bool ExplosionListener::protect(const endstone::Location &origin, ExplosionSource source,
                                std::vector<endstone::NotNull<endstone::Block>> &blocks)
{
    if (!allowsExplosion(source, [&](Flag flag) { return context_.allowed(origin, flag); })) {
        return false;
    }
    filterExplosionBlocks(
        blocks, source, [&](const auto &block, Flag flag) { return context_.allowed(block->getLocation(), flag); },
        [&](const auto &block) {
            if (source == ExplosionSource::Tnt) {
                return context_.allowsNonPlayerBlockChange(origin, block->getLocation());
            }
            if (source == ExplosionSource::Unknown) {
                return context_.allowsNonPlayerBlockChange(std::nullopt, block->getLocation());
            }
            return true;
        });
    return true;
}

bool ExplosionListener::onActorExplosion(endstone::ActorExplodeEvent &event)
{
    const auto id = event.getActor()->getType().getId();
    return protect(event.getLocation(), explosionSource({id.getNamespace(), id.getKey()}), event.getBlockList());
}

bool ExplosionListener::onBlockExplosion(endstone::BlockExplodeEvent &event)
{
    // The public block event also represents explosions whose actor could not be resolved.
    return protect(event.getBlock()->getLocation(), ExplosionSource::Unknown, event.getBlockList());
}

bool ExplosionListener::onDamage(endstone::ActorDamageEvent &event)
{
    const auto &damage = *event.getDamageSource();
    if (damage.getType() != "block_explosion" && damage.getType() != "entity_explosion") {
        return true;
    }
    const auto source =
        explosionDamageSource(damage.getType(), actorType(damage.getDamagingActor()), actorType(damage.getActor()));
    return allowsExplosion(*source, [&](Flag flag) { return context_.allowed(event.getActor()->getLocation(), flag); });
}

}
