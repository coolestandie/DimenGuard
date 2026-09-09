#include "dimenguard/listener/item_listener.h"

#include "dimenguard/protection/protection_context.h"

#include <endstone/event/actor/actor_pickup_item_event.h>
#include <endstone/event/player/player_drop_item_event.h>
#include <endstone/event/player/player_pickup_arrow_event.h>
#include <endstone/event/player/player_pickup_item_event.h>
#include <endstone/player.h>

namespace dimenguard {

ItemListener::ItemListener(ProtectionContext &context) : context_(context) {}

void ItemListener::registerEvents()
{
    context_.registerGuarded(&ItemListener::onPlayerDrop, *this);
    context_.registerGuarded(&ItemListener::onPlayerPickup, *this);
    context_.registerGuarded(&ItemListener::onArrowPickup, *this);
    context_.registerGuarded(&ItemListener::onActorPickup, *this);
}

bool ItemListener::onPlayerDrop(endstone::PlayerDropItemEvent &event)
{
    const auto &player = event.getPlayer();
    return context_.allowed(*player, player->getLocation(), Flag::ItemDrop);
}

bool ItemListener::playerPickupAllowed(endstone::Player &player, const endstone::Actor &item)
{
    return context_.allowedAtBoth(player, player.getLocation(), item.getLocation(), Flag::ItemPickup);
}

bool ItemListener::onPlayerPickup(endstone::PlayerPickupItemEvent &event)
{
    return playerPickupAllowed(*event.getPlayer(), *event.getItem());
}

bool ItemListener::onArrowPickup(endstone::PlayerPickupArrowEvent &event)
{
    return playerPickupAllowed(*event.getPlayer(), *event.getArrow());
}

bool ItemListener::onActorPickup(endstone::ActorPickupItemEvent &event)
{
    return context_.allowedAtBoth(event.getActor()->getLocation(), event.getItem()->getLocation(), Flag::ItemPickup);
}

}
