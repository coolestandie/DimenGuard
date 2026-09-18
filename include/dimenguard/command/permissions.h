#pragma once

#include "dimenguard/region/region.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace endstone {
class CommandSender;
}

namespace dimenguard {

enum class CommandPermission {
    Selection,
    Inspect,
    Create,
    Claim,
    Delete,
    Rename,
    Redefine,
    Move,
    List,
    Info,
    Flags,
    Priority,
    Parent,
    Passthrough,
    Membership,
    Ownership,
    Teleport,
    Recovery,
};

enum class RegionPermissionScope {
    Owner,
    Member,
};

[[nodiscard]] std::span<const CommandPermission> commandPermissions();
[[nodiscard]] std::string_view permissionName(CommandPermission permission);
[[nodiscard]] std::optional<CommandPermission> permissionForCommand(std::string_view path);

[[nodiscard]] bool hasCommandPermission(const endstone::CommandSender &sender, CommandPermission permission);
[[nodiscard]] bool hasAnyCommandPermission(const endstone::CommandSender &sender, CommandPermission permission);
[[nodiscard]] bool hasEmergencyRecovery(const endstone::CommandSender &sender);

[[nodiscard]] bool hasRegionPermission(const endstone::CommandSender &sender, CommandPermission permission,
                                       const Region &region);
[[nodiscard]] bool hasRegionFlagPermission(const endstone::CommandSender &sender, const Region &region, Flag flag,
                                           std::optional<std::string_view> value);
[[nodiscard]] bool canSeeRegion(const endstone::CommandSender &sender, const Region &region);

[[nodiscard]] std::string flagPermissionName(Flag flag);
[[nodiscard]] std::string flagValuePermissionName(Flag flag, std::string_view value);

}
