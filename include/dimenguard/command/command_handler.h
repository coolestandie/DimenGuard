#pragma once
#include "dimenguard/command/context.h"

#include <span>
#include <string>
namespace dimenguard {
class CommandHandler {
public:
    explicit CommandHandler(DimenGuardPlugin &plugin);
    void execute(endstone::CommandSender &sender, std::span<const std::string> args);

private:
    void dispatch(endstone::CommandSender &sender, std::span<const std::string> args);
    DimenGuardPlugin &plugin_;
    CommandContext context_;
};
}
