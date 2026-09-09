#include "dimenguard/listener/actor_listener.h"

#include "dimenguard/protection/protection_context.h"

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
    const auto attacker = event.getDamageSource()->getActor().as<endstone::Player>();
    if (!victim || !attacker || victim->getUniqueId() == attacker->getUniqueId()) {
        return true;
    }
    return context_.allowed(*attacker, attacker->getLocation(), Flag::Pvp) &&
           context_.allowed(*attacker, victim->getLocation(), Flag::Pvp);
}

}
