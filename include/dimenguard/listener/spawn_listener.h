#pragma once

namespace endstone {
class ActorSpawnEvent;
}

namespace dimenguard {

class ProtectionContext;

class SpawnListener {
public:
    explicit SpawnListener(ProtectionContext &context);
    void registerEvents();

private:
    bool onSpawn(endstone::ActorSpawnEvent &event);
    ProtectionContext &context_;
};

}
