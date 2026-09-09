#include "dimenguard/adapter/context.h"
#include "dimenguard/command/context.h"
#include "dimenguard/command/handlers.h"
namespace dimenguard {
void executeSelectionCommand(CommandContext &context, endstone::CommandSender &sender,
                             std::span<const std::string> args)
{
    requireArgument(args.size() == 1);
    static_cast<void>(context.service());
    auto &player = context.player(sender);
    const auto dimension = dimensionKey(*player.getDimension());
    const auto position = blockPosition(player.getLocation());
    const bool first = args[0] == "pos1";
    context.selections().set(player.getUniqueId().str(), dimension, position,
                             first ? SelectionCorner::First : SelectionCorner::Second);
    context.messages().send(player, Message::Selected, first ? 1 : 2, position.x, position.y, position.z,
                            dimension.dimension);
}
}
