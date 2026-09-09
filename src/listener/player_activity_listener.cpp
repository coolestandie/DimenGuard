#include "dimenguard/listener/player_activity_listener.h"

#include "dimenguard/protection/protection_context.h"

#include <endstone/event/player/player_bed_enter_event.h>
#include <endstone/event/player/player_chat_event.h>
#include <endstone/player.h>

namespace dimenguard {

PlayerActivityListener::PlayerActivityListener(ProtectionContext &context) : context_(context) {}

void PlayerActivityListener::registerEvents()
{
    context_.registerGuarded(&PlayerActivityListener::onSleep, *this);
    context_.registerGuarded(&PlayerActivityListener::onChat, *this);
}

bool PlayerActivityListener::onSleep(endstone::PlayerBedEnterEvent &event)
{
    return context_.allowed(*event.getPlayer(), event.getBed().getLocation(), Flag::Sleep);
}

bool PlayerActivityListener::onChat(endstone::PlayerChatEvent &event)
{
    const auto &player = event.getPlayer();
    return context_.allowed(*player, player->getLocation(), Flag::SendChat);
}

}
