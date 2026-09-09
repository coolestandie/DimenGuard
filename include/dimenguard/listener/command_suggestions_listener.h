#pragma once

#include <endstone/scheduler/task.h>
#include <optional>

namespace endstone {
class PacketSendEvent;
}
namespace dimenguard {
class DimenGuardPlugin;

class CommandSuggestionsListener {
public:
    explicit CommandSuggestionsListener(DimenGuardPlugin &plugin);
    ~CommandSuggestionsListener();
    void registerEvents();
    void requestRefresh() noexcept;

private:
    void onPacketSend(endstone::PacketSendEvent &event) noexcept;
    void refresh() noexcept;
    void reportFailure() noexcept;

    DimenGuardPlugin &plugin_;
    std::optional<endstone::TaskId> refresh_task_;
    bool supported_ = false;
    bool failure_reported_ = false;
};
}
