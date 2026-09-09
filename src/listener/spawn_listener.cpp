#include "dimenguard/listener/spawn_listener.h"

#include "dimenguard/protection/protection_context.h"

#include <endstone/actor/mob.h>
#include <endstone/event/actor/actor_spawn_event.h>
#include <endstone/player.h>
#include <string>

namespace dimenguard {

SpawnListener::SpawnListener(ProtectionContext &context) : context_(context) {}

void SpawnListener::registerEvents()
{
    context_.registerGuarded(&SpawnListener::onSpawn, *this);
}

bool SpawnListener::onSpawn(endstone::ActorSpawnEvent &event)
{
    const auto actor = event.getActor();
    return actor.is<endstone::Player>() ||
           context_.permitsSpawn(actor->getLocation(), static_cast<std::string>(actor->getType().getId()),
                                 actor.is<endstone::Mob>());
}

}
