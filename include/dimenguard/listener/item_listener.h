#pragma once

namespace endstone {
class Actor;
class ActorPickupItemEvent;
class Player;
class PlayerDropItemEvent;
class PlayerPickupArrowEvent;
class PlayerPickupItemEvent;
}

namespace dimenguard {

class ProtectionContext;

class ItemListener {
public:
    explicit ItemListener(ProtectionContext &context);
    void registerEvents();

private:
    bool playerPickupAllowed(endstone::Player &player, const endstone::Actor &item);
    bool onPlayerDrop(endstone::PlayerDropItemEvent &event);
    bool onPlayerPickup(endstone::PlayerPickupItemEvent &event);
    bool onArrowPickup(endstone::PlayerPickupArrowEvent &event);
    bool onActorPickup(endstone::ActorPickupItemEvent &event);
    ProtectionContext &context_;
};

}
