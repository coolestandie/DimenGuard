#include "dimenguard/listener/mob_listener.h"

#include "dimenguard/protection/protection_context.h"

#include <endstone/actor/mob.h>
#include <endstone/block/block.h>
#include <endstone/event/actor/actor_change_block_event.h>
#include <endstone/event/actor/actor_damage_event.h>
#include <endstone/event/actor/actor_spawn_event.h>
#include <endstone/player.h>

namespace dimenguard {

MobListener::MobListener(ProtectionContext &context) : context_(context) {}

void MobListener::registerEvents()
{
    context_.registerGuarded(&MobListener::onGriefing, *this);
    context_.registerGuarded(&MobListener::onSpawn, *this);
    context_.registerGuarded(&MobListener::onDamage, *this);
}

bool MobListener::onGriefing(endstone::ActorChangeBlockEvent &event)
{
    return context_.allowed(event.getBlock()->getLocation(), Flag::ActorGriefing);
}

bool MobListener::onSpawn(endstone::ActorSpawnEvent &event)
{
    const auto mob = event.getActor().as<endstone::Mob>();
    return !mob || mob.is<endstone::Player>() || context_.allowed(mob->getLocation(), Flag::MobSpawning);
}

bool MobListener::onDamage(endstone::ActorDamageEvent &event)
{
    const auto victim = event.getActor().as<endstone::Player>();
    const auto attacker = event.getDamageSource()->getActor().as<endstone::Mob>();
    return !victim || !attacker || attacker.is<endstone::Player>() ||
           context_.allowed(victim->getLocation(), Flag::MobDamage);
}

}
