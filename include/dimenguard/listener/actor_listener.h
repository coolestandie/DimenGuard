#pragma once

namespace endstone {
class ActorDamageEvent;
}

namespace dimenguard {

class ProtectionContext;

class ActorListener {
public:
    explicit ActorListener(ProtectionContext &context);
    void registerEvents();

private:
    bool onDamage(endstone::ActorDamageEvent &event);
    ProtectionContext &context_;
};

}
