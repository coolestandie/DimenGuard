#include "dimenguard/adapter/context.h"
#include "dimenguard/command/context.h"
#include "dimenguard/command/handlers.h"
#include "dimenguard/command/permissions.h"
namespace dimenguard {
void executeSelectionCommand(CommandContext &context, endstone::CommandSender &sender,
                             std::span<const std::string> args)
{
    requireArgument(args.size() == 1);
    static_cast<void>(context.service());
    auto &player = context.player(sender);
    const auto dimension = dimensionKey(*player.getDimension());
    const auto position = blockPosition(player.getLocation());
    if (args[0] == "inspect") {
        requireArgument(hasCommandPermission(sender, CommandPermission::Inspect), Message::NoPermission);
        const auto regions = context.service().getRegions().query(dimension, position);
        context.messages().send(sender, Message::Inspect, position.x, position.y, position.z, dimension.dimension,
                                regions.size());
        for (const auto *region : regions) {
            context.messages().send(sender, Message::InspectItem, region->key.name, regionKindName(region->kind),
                                    region->priority, region->owner);
        }
        return;
    }
    requireArgument(hasCommandPermission(sender, CommandPermission::Selection), Message::NoPermission);
    const bool first = args[0] == "pos1";
    context.selections().set(player.getUniqueId().str(), dimension, position,
                             first ? SelectionCorner::First : SelectionCorner::Second);
    context.messages().send(player, Message::Selected, first ? 1 : 2, position.x, position.y, position.z,
                            dimension.dimension);
}
}
