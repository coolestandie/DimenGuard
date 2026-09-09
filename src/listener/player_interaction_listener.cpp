#include "dimenguard/listener/player_interaction_listener.h"

#include "dimenguard/protection/protection_context.h"

#include <endstone/block/container.h>
#include <endstone/event/player/player_armor_stand_manipulate_event.h>
#include <endstone/event/player/player_bucket_empty_event.h>
#include <endstone/event/player/player_bucket_fill_event.h>
#include <endstone/event/player/player_interact_actor_event.h>
#include <endstone/event/player/player_interact_event.h>

namespace dimenguard {

PlayerInteractionListener::PlayerInteractionListener(ProtectionContext &context) : context_(context) {}

void PlayerInteractionListener::registerEvents()
{
    context_.registerGuarded(&PlayerInteractionListener::onBucketFill, *this);
    context_.registerGuarded(&PlayerInteractionListener::onBucketEmpty, *this);
    context_.registerGuarded(&PlayerInteractionListener::onInteract, *this);
    context_.registerGuarded(&PlayerInteractionListener::onInteractActor, *this);
    context_.registerGuarded(&PlayerInteractionListener::onArmorStandManipulate, *this);
}

bool PlayerInteractionListener::bucketAllowed(endstone::PlayerBucketEvent &event)
{
    const auto &target = event.getBlock();
    const auto &block = target ? *target : *event.getBlockClicked();
    return context_.allowed(*event.getPlayer(), block.getLocation(), Flag::Build);
}

bool PlayerInteractionListener::onBucketFill(endstone::PlayerBucketFillEvent &event)
{
    return bucketAllowed(event);
}

bool PlayerInteractionListener::onBucketEmpty(endstone::PlayerBucketEmptyEvent &event)
{
    return bucketAllowed(event);
}

bool PlayerInteractionListener::onInteract(endstone::PlayerInteractEvent &event)
{
    if (event.getAction() != endstone::PlayerInteractEvent::Action::RightClickBlock || !event.getBlock()) {
        return true;
    }
    const auto &block = *event.getBlock();
    const auto container = block.captureState(false).as<endstone::Container>();
    const auto flag = container ? Flag::ContainerAccess : Flag::Interact;
    return context_.allowed(*event.getPlayer(), block.getLocation(), flag) &&
           (!container || context_.chestNeighbors(*event.getPlayer(), block));
}

bool PlayerInteractionListener::onInteractActor(endstone::PlayerInteractActorEvent &event)
{
    return context_.allowed(*event.getPlayer(), event.getActor()->getLocation(), Flag::Interact);
}

bool PlayerInteractionListener::onArmorStandManipulate(endstone::PlayerArmorStandManipulateEvent &event)
{
    return onInteractActor(event);
}

}
