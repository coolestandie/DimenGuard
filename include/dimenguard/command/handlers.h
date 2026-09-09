#pragma once
#include <span>
#include <string>
namespace endstone {
class CommandSender;
}
namespace dimenguard {
class CommandContext;
void executeGeneralCommand(CommandContext &context, endstone::CommandSender &sender, std::span<const std::string> args);
void executeSelectionCommand(CommandContext &context, endstone::CommandSender &sender,
                             std::span<const std::string> args);
void executeRegionCommand(CommandContext &context, endstone::CommandSender &sender, std::span<const std::string> args);
void executeFlagCommand(CommandContext &context, endstone::CommandSender &sender, std::span<const std::string> args);
void executeMembershipCommand(CommandContext &context, endstone::CommandSender &sender,
                              std::span<const std::string> args);
}
