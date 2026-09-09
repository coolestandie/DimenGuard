#include "dimenguard/listener/protection_listener.h"

namespace dimenguard {

ProtectionListener::ProtectionListener(DimenGuardPlugin &plugin)
    : context_(plugin), blocks_(context_), player_interactions_(context_), actors_(context_),
      sessions_(plugin, context_), world_blocks_(context_), explosions_(context_), mobs_(context_), movements_(context_)
{
}

void ProtectionListener::registerEvents()
{
    blocks_.registerEvents();
    player_interactions_.registerEvents();
    actors_.registerEvents();
    sessions_.registerEvents();
    world_blocks_.registerEvents();
    explosions_.registerEvents();
    mobs_.registerEvents();
    movements_.registerEvents();
}

}
