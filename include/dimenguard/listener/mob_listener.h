#pragma once

namespace endstone {
class ActorChangeBlockEvent;
class ActorDamageEvent;
}

namespace dimenguard {

class ProtectionContext;

class MobListener {
public:
    explicit MobListener(ProtectionContext &context);
    void registerEvents();

private:
    bool onGriefing(endstone::ActorChangeBlockEvent &event);
    bool onDamage(endstone::ActorDamageEvent &event);

    ProtectionContext &context_;
};

}
