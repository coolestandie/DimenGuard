#pragma once

namespace endstone {
class PlayerQuitEvent;
}

namespace dimenguard {

class DimenGuardPlugin;
class ProtectionContext;

class PlayerSessionListener {
public:
    PlayerSessionListener(DimenGuardPlugin &plugin, ProtectionContext &context);
    void registerEvents();

private:
    void onQuit(endstone::PlayerQuitEvent &event);
    DimenGuardPlugin &plugin_;
    ProtectionContext &context_;
};

}
