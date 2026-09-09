#pragma once

namespace endstone {
class ActorChangeBlockEvent;
class ActorDamageEvent;
class ActorSpawnEvent;
}

namespace dimenguard {

class ProtectionContext;

class MobListener {
public:
    explicit MobListener(ProtectionContext &context);
    void registerEvents();

private:
    bool onGriefing(endstone::ActorChangeBlockEvent &event);
    bool onSpawn(endstone::ActorSpawnEvent &event);
    bool onDamage(endstone::ActorDamageEvent &event);

    ProtectionContext &context_;
};

}
