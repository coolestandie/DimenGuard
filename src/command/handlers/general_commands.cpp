#include "dimenguard/command/context.h"
#include "dimenguard/command/handlers.h"
namespace dimenguard {
void executeGeneralCommand(CommandContext &context, endstone::CommandSender &sender, std::span<const std::string> args)
{
    auto &messages = context.messages();
    if (args.empty() || args[0] == "help") {
        requireArgument(args.size() <= 1);
        messages.sendHelp(sender);
    }
    else if (args[0] == "language") {
        requireArgument(args.size() == 2 && (args[1] == "en" || args[1] == "es"));
        auto &player = context.player(sender);
        messages.setLocale(player, parseLocale(args[1]));
        messages.send(player, Message::LanguageSet, args[1]);
    }
    else {
        requireArgument(args.size() == 1 && args[0] == "reload");
        context.reload();
        messages.send(sender, Message::Reloaded, context.service().getRegions().getAll().size());
    }
}
}
