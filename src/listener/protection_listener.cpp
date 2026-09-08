#include "dimenguard/listener/protection_listener.h"

#include "dimenguard/adapter/context.h"
#include "dimenguard/plugin.h"
#include "dimenguard/region/coordinates.h"

#include <array>
#include <endstone/block/container.h>
#include <endstone/event/actor/actor_damage_event.h>
#include <endstone/event/block/block_break_event.h>
#include <endstone/event/block/block_place_event.h>
#include <endstone/event/event_priority.h>
#include <endstone/event/player/player_armor_stand_manipulate_event.h>
#include <endstone/event/player/player_bucket_empty_event.h>
#include <endstone/event/player/player_bucket_fill_event.h>
#include <endstone/event/player/player_interact_actor_event.h>
#include <endstone/event/player/player_interact_event.h>
#include <exception>
#include <stdexcept>

namespace dimenguard {
namespace {

constexpr auto error_interval = std::chrono::seconds(10);

}  // namespace

ProtectionListener::ProtectionListener(DimenGuardPlugin &plugin) : plugin_(plugin) {}

template <typename EventType>
void ProtectionListener::registerGuarded(bool (ProtectionListener::*handler)(EventType &))
{
    plugin_.registerEvent<EventType>(
        [this, handler](EventType &event) {
            try {
                if (!(this->*handler)(event)) {
                    event.setCancelled(true);
                }
            }
            catch (const std::exception &error) {
                event.setCancelled(true);
                reportFailure(error.what());
            }
            catch (...) {
                event.setCancelled(true);
                reportFailure("Unknown protection error");
            }
        },
        endstone::EventPriority::High, true);
}

void ProtectionListener::registerEvents()
{
    registerGuarded(&ProtectionListener::onBlockBreak);
    registerGuarded(&ProtectionListener::onBlockPlace);
    registerGuarded(&ProtectionListener::onBucketFill);
    registerGuarded(&ProtectionListener::onBucketEmpty);
    registerGuarded(&ProtectionListener::onInteract);
    registerGuarded(&ProtectionListener::onInteractActor);
    registerGuarded(&ProtectionListener::onArmorStandManipulate);
    registerGuarded(&ProtectionListener::onActorDamage);
}

bool ProtectionListener::allowed(endstone::Player &player, const endstone::Location &location, Flag flag)
{
    const auto *service = plugin_.getService();
    if (!service) {
        plugin_.getMessenger().deny(player, Message::NotReady);
        return false;
    }
    const auto dimension = location.getDimension();
    if (!dimension) {
        throw std::invalid_argument("Protection target has no dimension");
    }
    if (service->getRegions().isAllowed(dimensionKey(*dimension), blockPosition(location), flag,
                                        player.getUniqueId().str(), player.hasPermission("dimenguard.bypass"))) {
        return true;
    }
    plugin_.getMessenger().deny(player);
    return false;
}

bool ProtectionListener::allowedChestNeighbors(endstone::Player &player, const endstone::Block &block)
{
    const auto type = block.getType().getId();
    if (type != endstone::BlockTypeId::minecraft("chest") &&
        type != endstone::BlockTypeId::minecraft("trapped_chest")) {
        return true;
    }
    const auto dimension = block.getDimension();
    constexpr std::array<BlockPosition, 4> offsets{{{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}}};
    for (const auto &offset : offsets) {
        const auto x = checkedOffset(block.getX(), offset.x);
        const auto z = checkedOffset(block.getZ(), offset.z);
        if (!dimension->isChunkLoaded(chunkCoordinate(x), chunkCoordinate(z))) {
            throw std::runtime_error("Cannot verify an adjacent chest in an unloaded chunk");
        }
        const auto neighbor = dimension->getBlockAt(x, block.getY(), z);
        // Public Endstone does not expose chest pairing. Same-type neighbors are conservative
        // candidates, so an adjacent but unpaired protected chest can also deny this action.
        if (neighbor->getType().getId() == type && !allowed(player, neighbor->getLocation(), Flag::ContainerAccess)) {
            return false;
        }
    }
    return true;
}

bool ProtectionListener::allowedBucket(endstone::PlayerBucketEvent &event)
{
    const auto &target = event.getBlock();
    const auto &block = target ? *target : *event.getBlockClicked();
    return allowed(*event.getPlayer(), block.getLocation(), Flag::Build);
}

void ProtectionListener::reportFailure(std::string_view message) noexcept
{
    const auto now = std::chrono::steady_clock::now();
    if (error_reported_ && now - last_error_ < error_interval) {
        return;
    }
    error_reported_ = true;
    last_error_ = now;
    try {
        plugin_.getLogger().error("Protection listener failed: {}", message);
    }
    catch (...) {
        // Logging must never escape an event handler after its action has been cancelled.
    }
}

bool ProtectionListener::onBlockBreak(endstone::BlockBreakEvent &event)
{
    return allowed(*event.getPlayer(), event.getBlock()->getLocation(), Flag::Build);
}

bool ProtectionListener::onBlockPlace(endstone::BlockPlaceEvent &event)
{
    const auto &block = *event.getBlockPlaced();
    return allowed(*event.getPlayer(), block.getLocation(), Flag::Build) &&
           allowedChestNeighbors(*event.getPlayer(), block);
}

bool ProtectionListener::onBucketFill(endstone::PlayerBucketFillEvent &event)
{
    return allowedBucket(event);
}

bool ProtectionListener::onBucketEmpty(endstone::PlayerBucketEmptyEvent &event)
{
    return allowedBucket(event);
}

bool ProtectionListener::onInteract(endstone::PlayerInteractEvent &event)
{
    if (event.getAction() != endstone::PlayerInteractEvent::Action::RightClickBlock || !event.getBlock()) {
        return true;
    }
    const auto &block = *event.getBlock();
    const auto container = block.captureState(false).as<endstone::Container>();
    const auto flag = container ? Flag::ContainerAccess : Flag::Interact;
    return allowed(*event.getPlayer(), block.getLocation(), flag) &&
           (!container || allowedChestNeighbors(*event.getPlayer(), block));
}

bool ProtectionListener::onInteractActor(endstone::PlayerInteractActorEvent &event)
{
    return allowed(*event.getPlayer(), event.getActor()->getLocation(), Flag::Interact);
}

bool ProtectionListener::onArmorStandManipulate(endstone::PlayerArmorStandManipulateEvent &event)
{
    return onInteractActor(event);
}

bool ProtectionListener::onActorDamage(endstone::ActorDamageEvent &event)
{
    const auto victim = event.getActor().as<endstone::Player>();
    const auto attacker = event.getDamageSource()->getActor().as<endstone::Player>();
    if (!victim || !attacker || victim->getUniqueId() == attacker->getUniqueId()) {
        return true;
    }
    return allowed(*attacker, attacker->getLocation(), Flag::Pvp) &&
           allowed(*attacker, victim->getLocation(), Flag::Pvp);
}

}  // namespace dimenguard
