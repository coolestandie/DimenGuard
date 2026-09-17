#pragma once
#include "dimenguard/command/selection.h"
#include "dimenguard/presentation/messenger.h"
#include "dimenguard/service/region_service.h"

#include <exception>
#include <string_view>
namespace endstone {
class Server;
}
namespace dimenguard {
class DimenGuardPlugin;
class CommandError : public std::exception {
public:
    explicit CommandError(Message message) : message_(message) {}
    [[nodiscard]] Message getMessage() const { return message_; }

private:
    Message message_;
};
void requireArgument(bool condition, Message message = Message::Usage);
class CommandContext {
public:
    explicit CommandContext(DimenGuardPlugin &plugin);
    [[nodiscard]] Messenger &messages() const;
    [[nodiscard]] RegionService &service() const;
    [[nodiscard]] SelectionManager &selections() const;
    [[nodiscard]] endstone::Server &server() const;
    [[nodiscard]] endstone::Player &player(endstone::CommandSender &sender) const;
    [[nodiscard]] const Region &region(const endstone::Player &player, const std::string &name) const;
    [[nodiscard]] endstone::NotNull<endstone::Player> resolvePlayer(std::string_view name) const;
    void reload() const;

private:
    DimenGuardPlugin &plugin_;
};
}
