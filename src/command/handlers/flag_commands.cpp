#include "dimenguard/command/context.h"
#include "dimenguard/command/handlers.h"
#include "dimenguard/presentation/flag_panel.h"
namespace dimenguard {
void executeFlagCommand(CommandContext &context, endstone::CommandSender &sender, std::span<const std::string> args)
{
    requireArgument(args.size() <= 3);
    auto &messages = context.messages();
    const auto locale = messages.getLocale(sender);
    if (args.empty()) {
        messages.sendLines(sender, renderFlagCatalog(locale));
        return;
    }
    auto &service = context.service();
    auto &player = context.player(sender);
    const auto &region = context.region(player, args[0]);
    std::optional<Flag> selected;
    if (args.size() >= 2) {
        selected = parseFlag(args[1]);
        requireArgument(selected.has_value(), Message::InvalidFlag);
    }
    if (args.size() == 3) {
        const auto state = parseState(args[2]);
        requireArgument(state.has_value(), Message::InvalidFlag);
        service.setFlag(region.key, *selected, *state);
        messages.send(sender, Message::FlagSet, args[0], args[1], args[2]);
    }
    else {
        messages.sendLines(sender, renderRegionFlags(region, locale, selected));
    }
}
}
