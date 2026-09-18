#include "dimenguard/command/permissions.h"

#include <endstone/command/command_sender.h>
#include <endstone/player.h>
#include <format>

namespace dimenguard {
namespace {

std::string scopedPermissionName(CommandPermission permission, RegionPermissionScope scope)
{
    return std::format("{}.{}", permissionName(permission), scope == RegionPermissionScope::Owner ? "own" : "member");
}

bool hasExplicitPermission(const endstone::CommandSender &sender, std::string_view name)
{
    const std::string owned{name};
    return sender.isPermissionSet(owned) && sender.hasPermission(owned);
}

bool hasRegionScopePermission(const endstone::CommandSender &sender, CommandPermission permission,
                              RegionPermissionScope scope, std::string_view region_name)
{
    const auto scoped = scopedPermissionName(permission, scope);
    if (sender.hasPermission(scoped)) {
        return true;
    }
    return hasExplicitPermission(sender, std::format("{}.{}", scoped, region_name));
}

}

bool hasCommandPermission(const endstone::CommandSender &sender, CommandPermission permission)
{
    return sender.hasPermission(std::string(permissionName(permission)));
}

bool hasAnyCommandPermission(const endstone::CommandSender &sender, CommandPermission permission)
{
    if (hasCommandPermission(sender, permission)) {
        return true;
    }
    return sender.hasPermission(scopedPermissionName(permission, RegionPermissionScope::Owner)) ||
           sender.hasPermission(scopedPermissionName(permission, RegionPermissionScope::Member)) ||
           sender.hasPermission(std::format("{}.region.*", permissionName(permission)));
}

bool hasEmergencyRecovery(const endstone::CommandSender &sender)
{
    return hasCommandPermission(sender, CommandPermission::Recovery);
}

bool hasRegionPermission(const endstone::CommandSender &sender, CommandPermission permission, const Region &region)
{
    if (hasCommandPermission(sender, permission)) {
        return true;
    }
    if (hasExplicitPermission(sender, std::format("{}.region.{}", permissionName(permission), region.key.name))) {
        return true;
    }
    const auto player = sender.as<endstone::Player>();
    if (player == nullptr) {
        return false;
    }
    const auto player_id = player->getUniqueId().str();
    if (region.owner == player_id &&
        hasRegionScopePermission(sender, permission, RegionPermissionScope::Owner, region.key.name)) {
        return true;
    }
    if (region.members.contains(player_id) &&
        hasRegionScopePermission(sender, permission, RegionPermissionScope::Member, region.key.name)) {
        return true;
    }
    return false;
}

bool hasRegionFlagPermission(const endstone::CommandSender &sender, const Region &region, Flag flag,
                             std::optional<std::string_view> value)
{
    if (hasCommandPermission(sender, CommandPermission::Flags)) {
        return true;
    }
    if (!hasRegionPermission(sender, CommandPermission::Flags, region)) {
        return false;
    }
    const auto flag_permission = flagPermissionName(flag);
    const auto any_flag_permission = std::string{"dimenguard.region.flag.*"};
    if (!sender.hasPermission(flag_permission) && !sender.hasPermission(any_flag_permission)) {
        return false;
    }
    if (!value) {
        return true;
    }
    return sender.hasPermission(flagValuePermissionName(flag, *value)) || sender.hasPermission(any_flag_permission);
}

bool canSeeRegion(const endstone::CommandSender &sender, const Region &region)
{
    for (const auto permission :
         {CommandPermission::List, CommandPermission::Info, CommandPermission::Flags, CommandPermission::Selection,
          CommandPermission::Delete, CommandPermission::Rename, CommandPermission::Redefine, CommandPermission::Move,
          CommandPermission::Priority, CommandPermission::Parent, CommandPermission::Passthrough,
          CommandPermission::Membership, CommandPermission::Ownership}) {
        if (hasRegionPermission(sender, permission, region)) {
            return true;
        }
    }
    return false;
}

}
