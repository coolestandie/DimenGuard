#pragma once

#include "dimenguard/region/region.h"

#include <chrono>
#include <endstone/plugin/plugin.h>
#include <exception>
#include <string_view>
#include <type_traits>

namespace endstone {
class Block;
class Location;
class Player;
}

namespace dimenguard {

class DimenGuardPlugin;

class ProtectionContext {
public:
    explicit ProtectionContext(DimenGuardPlugin &plugin);
    [[nodiscard]] bool allowed(endstone::Player &player, const endstone::Location &location, Flag flag);
    [[nodiscard]] bool allowed(const endstone::Location &location, Flag flag);
    [[nodiscard]] bool allowedTransition(endstone::Player &player, const endstone::Location &from,
                                         const endstone::Location &to);
    [[nodiscard]] bool chestNeighbors(endstone::Player &player, const endstone::Block &block);
    void reportFailure(std::string_view message) noexcept;

    template <typename EventType, typename Listener, typename Result>
    void registerGuarded(Result (Listener::*handler)(EventType &), Listener &listener,
                         endstone::EventPriority priority = endstone::EventPriority::High, bool ignore_cancelled = true)
    {
        static_assert(std::is_same_v<Result, bool> || std::is_same_v<Result, void>);
        getPlugin().registerEvent<EventType>(
            [this, handler, &listener](EventType &event) {
                try {
                    if constexpr (std::is_same_v<Result, bool>) {
                        if (!(listener.*handler)(event)) {
                            event.setCancelled(true);
                        }
                    }
                    else {
                        (listener.*handler)(event);
                    }
                }
                catch (const std::exception &error) {
                    cancelOnFailure(event);
                    reportFailure(error.what());
                }
                catch (...) {
                    cancelOnFailure(event);
                    reportFailure("Unknown protection error");
                }
            },
            priority, ignore_cancelled);
    }

private:
    template <typename EventType>
    static void cancelOnFailure(EventType &event)
    {
        if constexpr (requires { event.setCancelled(true); }) {
            event.setCancelled(true);
        }
    }

    [[nodiscard]] endstone::Plugin &getPlugin();
    DimenGuardPlugin &plugin_;
    std::chrono::steady_clock::time_point last_error_{};
    bool error_reported_ = false;
};

}
