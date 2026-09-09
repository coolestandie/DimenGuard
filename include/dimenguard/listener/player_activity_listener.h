#pragma once

namespace endstone {
class PlayerBedEnterEvent;
class PlayerChatEvent;
}

namespace dimenguard {

class ProtectionContext;

class PlayerActivityListener {
public:
    explicit PlayerActivityListener(ProtectionContext &context);
    void registerEvents();

private:
    bool onSleep(endstone::PlayerBedEnterEvent &event);
    bool onChat(endstone::PlayerChatEvent &event);
    ProtectionContext &context_;
};

}
