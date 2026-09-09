#pragma once

namespace endstone {
class PlayerMoveEvent;
class PlayerTeleportEvent;
}

namespace dimenguard {

class ProtectionContext;

class PlayerMovementListener {
public:
    explicit PlayerMovementListener(ProtectionContext &context);
    void registerEvents();

private:
    bool onMove(endstone::PlayerMoveEvent &event);
    bool onTeleport(endstone::PlayerTeleportEvent &event);

    ProtectionContext &context_;
};

}
