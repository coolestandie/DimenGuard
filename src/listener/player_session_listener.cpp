#include "dimenguard/listener/player_session_listener.h"

#include "dimenguard/plugin.h"
#include "dimenguard/protection/protection_context.h"

#include <endstone/event/player/player_quit_event.h>

namespace dimenguard {

PlayerSessionListener::PlayerSessionListener(DimenGuardPlugin &plugin, ProtectionContext &context)
    : plugin_(plugin), context_(context)
{
}

void PlayerSessionListener::registerEvents()
{
    context_.registerGuarded(&PlayerSessionListener::onQuit, *this, endstone::EventPriority::Normal, false);
}

void PlayerSessionListener::onQuit(endstone::PlayerQuitEvent &event)
{
    plugin_.getMessenger().forget(*event.getPlayer());
    plugin_.getSelections().forget(event.getPlayer()->getUniqueId().str());
}

}
