#include "dimenguard/listener/actor_listener.h"

#include "dimenguard/protection/protection_context.h"
#include "dimenguard/rules/damage_rules.h"

#include <endstone/actor/mob.h>
#include <endstone/event/actor/actor_damage_event.h>
#include <endstone/player.h>

namespace dimenguard {

ActorListener::ActorListener(ProtectionContext &context) : context_(context) {}

void ActorListener::registerEvents()
{
    context_.registerGuarded(&ActorListener::onDamage, *this);
}

bool ActorListener::onDamage(endstone::ActorDamageEvent &event)
{
    const auto victim = event.getActor().as<endstone::Player>();
    const auto damage_flag = damageFlag(event.getDamageSource()->getType());
    if (!context_.permitsDamage(event.getActor()->getLocation(), static_cast<bool>(victim), damage_flag)) {
        return false;
    }
    const auto attacker = event.getDamageSource()->getActor().as<endstone::Player>();
    if (!victim || !attacker || victim->getUniqueId() == attacker->getUniqueId()) {
        return true;
    }
    return context_.allowedAtBoth(*attacker, attacker->getLocation(), victim->getLocation(), Flag::Pvp);
}

}
