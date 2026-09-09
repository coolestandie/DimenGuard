#pragma once

#include <endstone/command/command_sender.h>
#include <endstone/player.h>
#include <span>
#include <string>
#include <string_view>

namespace dimenguard {

class DimenGuardPlugin;
struct Region;

class CommandHandler {
public:
    explicit CommandHandler(DimenGuardPlugin &plugin);
    void execute(endstone::CommandSender &sender, std::span<const std::string> args);

private:
    void dispatch(endstone::CommandSender &sender, std::span<const std::string> args);
    void region(endstone::Player &player, std::span<const std::string> args);
    void flags(endstone::CommandSender &sender, std::span<const std::string> args);
    void listRegions(endstone::Player &player, std::span<const std::string> args);
    void showRegion(endstone::Player &player, const std::string &name);
    [[nodiscard]] const Region &findRegion(const endstone::Player &player, const std::string &name) const;
    [[nodiscard]] endstone::NotNull<endstone::Player> resolvePlayer(std::string_view argument) const;

    DimenGuardPlugin &plugin_;
};

}  // namespace dimenguard
