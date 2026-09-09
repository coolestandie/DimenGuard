#include "dimenguard/listener/player_movement_listener.h"

#include "dimenguard/protection/protection_context.h"

#include <endstone/event/player/player_move_event.h>
#include <endstone/event/player/player_teleport_event.h>

namespace dimenguard {

PlayerMovementListener::PlayerMovementListener(ProtectionContext &context) : context_(context) {}

void PlayerMovementListener::registerEvents()
{
    context_.registerGuarded(&PlayerMovementListener::onMove, *this);
    context_.registerGuarded(&PlayerMovementListener::onTeleport, *this);
}

bool PlayerMovementListener::onMove(endstone::PlayerMoveEvent &event)
{
    return context_.allowedTransition(*event.getPlayer(), event.getFrom(), event.getTo());
}

bool PlayerMovementListener::onTeleport(endstone::PlayerTeleportEvent &event)
{
    return onMove(event);
}

}
