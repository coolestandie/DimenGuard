#include "dimenguard/command/context.h"
#include "dimenguard/command/handlers.h"
#include "dimenguard/command/parse.h"
#include "dimenguard/presentation/flag_panel.h"
namespace dimenguard {
void executeGeneralCommand(CommandContext &context, endstone::CommandSender &sender, std::span<const std::string> args)
{
    if (!args.empty() && args[0] == "flags") {
        requireArgument(args.size() <= 2);
        const auto page = args.size() == 1 ? std::optional<int>{1} : parseInteger(args[1]);
        requireArgument(page && *page > 0, Message::InvalidPage);
        const auto panel = renderFlagCatalogPage(context.messages().getLocale(sender), static_cast<std::size_t>(*page),
                                                 sender.hasPermission("dimenguard.command"));
        requireArgument(panel.has_value(), Message::InvalidPage);
        context.messages().sendLines(sender, panel->lines);
        return;
    }
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
