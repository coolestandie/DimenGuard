#include "dimenguard/command/context.h"
#include "dimenguard/command/flag_input.h"
#include "dimenguard/command/handlers.h"
#include "dimenguard/command/permissions.h"
#include "dimenguard/presentation/flag_panel.h"
namespace dimenguard {
void executeFlagCommand(CommandContext &context, endstone::CommandSender &sender, std::span<const std::string> args)
{
    requireArgument(args.size() <= 3);
    auto &messages = context.messages();
    const auto locale = messages.getLocale(sender);
    if (args.empty()) {
        requireArgument(hasAnyCommandPermission(sender, CommandPermission::Flags), Message::NoPermission);
        messages.sendLines(sender, renderFlagCatalog(locale));
        return;
    }
    auto &service = context.service();
    auto &player = context.player(sender);
    const auto &region = context.region(player, args[0]);
    requireArgument(hasRegionPermission(sender, CommandPermission::Flags, region) || hasEmergencyRecovery(sender),
                    Message::NoPermission);
    std::optional<Flag> selected;
    if (args.size() >= 2) {
        selected = parseFlag(args[1]);
        requireArgument(selected.has_value(), Message::InvalidFlag);
    }
    if (args.size() == 3) {
        const auto change = parseFlagChange(*selected, args[2]);
        requireArgument(change.has_value(), Message::InvalidFlagValue);
        const auto value_permission = change->value && change->value->state()
                                        ? std::string_view{stateName(*change->value->state())}
                                    : change->value        ? std::string_view{"set"}
                                    : args[2] == "inherit" ? std::string_view{"inherit"}
                                                           : std::string_view{"unset"};
        requireArgument(hasRegionFlagPermission(sender, region, *selected, value_permission) ||
                            hasEmergencyRecovery(sender),
                        Message::NoPermission);
        service.setFlagValue(region.key, *selected, change->value);
        if (!change->value) {
            messages.send(sender, Message::FlagCleared, args[0], args[1]);
        }
        else {
            messages.send(sender, Message::FlagSet, args[0], args[1], displayFlagValue(*change->value));
        }
    }
    else {
        messages.sendLines(sender, renderRegionFlags(region, locale, selected));
    }
}
}
