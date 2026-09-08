#pragma once

#include "dimenguard/region/region.h"

#include <chrono>
#include <string_view>

namespace endstone {
class ActorDamageEvent;
class Block;
class BlockBreakEvent;
class BlockPlaceEvent;
class Location;
class Player;
class PlayerArmorStandManipulateEvent;
class PlayerBucketEmptyEvent;
class PlayerBucketEvent;
class PlayerBucketFillEvent;
class PlayerInteractActorEvent;
class PlayerInteractEvent;
}  // namespace endstone

namespace dimenguard {

class DimenGuardPlugin;

/** Translates synchronous public events into the shared region policy. */
class ProtectionListener {
public:
    explicit ProtectionListener(DimenGuardPlugin &plugin);
    void registerEvents();

private:
    template <typename EventType>
    void registerGuarded(bool (ProtectionListener::*handler)(EventType &));

    [[nodiscard]] bool allowed(endstone::Player &player, const endstone::Location &location, Flag flag);
    [[nodiscard]] bool allowedChestNeighbors(endstone::Player &player, const endstone::Block &block);
    [[nodiscard]] bool allowedBucket(endstone::PlayerBucketEvent &event);
    void reportFailure(std::string_view message) noexcept;

    bool onBlockBreak(endstone::BlockBreakEvent &event);
    bool onBlockPlace(endstone::BlockPlaceEvent &event);
    bool onBucketFill(endstone::PlayerBucketFillEvent &event);
    bool onBucketEmpty(endstone::PlayerBucketEmptyEvent &event);
    bool onInteract(endstone::PlayerInteractEvent &event);
    bool onInteractActor(endstone::PlayerInteractActorEvent &event);
    bool onArmorStandManipulate(endstone::PlayerArmorStandManipulateEvent &event);
    bool onActorDamage(endstone::ActorDamageEvent &event);

    DimenGuardPlugin &plugin_;
    std::chrono::steady_clock::time_point last_error_{};
    bool error_reported_ = false;
};

}  // namespace dimenguard
