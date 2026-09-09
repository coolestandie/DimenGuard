#pragma once

namespace endstone {
class BlockFormEvent;
class BlockFromToEvent;
class LeavesDecayEvent;
}

namespace dimenguard {

class ProtectionContext;

class WorldBlockListener {
public:
    explicit WorldBlockListener(ProtectionContext &context);
    void registerEvents();

private:
    bool onFlow(endstone::BlockFromToEvent &event);
    bool onForm(endstone::BlockFormEvent &event);
    bool onLeafDecay(endstone::LeavesDecayEvent &event);

    ProtectionContext &context_;
};

}
