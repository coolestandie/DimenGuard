#include "dimenguard/adapter/context.h"
#include "dimenguard/command/context.h"
#include "dimenguard/command/handlers.h"
#include "dimenguard/command/permissions.h"
namespace dimenguard {
void executeMembershipCommand(CommandContext &context, endstone::CommandSender &sender,
                              std::span<const std::string> args)
{
    requireArgument(args.size() == 3);
    auto &service = context.service();
    auto &player = context.player(sender);
    const auto &region = context.region(player, args[1]);
    requireArgument(hasRegionPermission(sender, CommandPermission::Membership, region) || hasEmergencyRecovery(sender),
                    Message::NoPermission);
    const auto target = context.resolvePlayer(args[2]);
    service.setMember({dimensionKey(*player.getDimension()), args[1]}, target->getUniqueId().str(), args[0] == "trust");
    context.messages().send(player, Message::MemberSet, args[1], target->getName());
}
}
