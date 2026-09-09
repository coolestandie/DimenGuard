#pragma once

namespace endstone {
class BlockBreakEvent;
class BlockPlaceEvent;
}

namespace dimenguard {

class ProtectionContext;

class BlockListener {
public:
    explicit BlockListener(ProtectionContext &context);
    void registerEvents();

private:
    bool onBreak(endstone::BlockBreakEvent &event);
    bool onPlace(endstone::BlockPlaceEvent &event);
    ProtectionContext &context_;
};

}
