#include "dimenguard/command/permissions.h"

#include <array>
#include <format>
#include <stdexcept>

namespace dimenguard {
namespace {

struct PermissionDefinition {
    CommandPermission permission;
    std::string_view name;
};

constexpr std::array definitions{
    PermissionDefinition{CommandPermission::Selection, "dimenguard.region.selection"},
    PermissionDefinition{CommandPermission::Inspect, "dimenguard.region.inspect"},
    PermissionDefinition{CommandPermission::Create, "dimenguard.region.create"},
    PermissionDefinition{CommandPermission::Claim, "dimenguard.region.claim"},
    PermissionDefinition{CommandPermission::Delete, "dimenguard.region.delete"},
    PermissionDefinition{CommandPermission::Rename, "dimenguard.region.rename"},
    PermissionDefinition{CommandPermission::Redefine, "dimenguard.region.redefine"},
    PermissionDefinition{CommandPermission::Move, "dimenguard.region.move"},
    PermissionDefinition{CommandPermission::List, "dimenguard.region.list"},
    PermissionDefinition{CommandPermission::Info, "dimenguard.region.info"},
    PermissionDefinition{CommandPermission::Flags, "dimenguard.region.flags"},
    PermissionDefinition{CommandPermission::Priority, "dimenguard.region.priority"},
    PermissionDefinition{CommandPermission::Parent, "dimenguard.region.parent"},
    PermissionDefinition{CommandPermission::Passthrough, "dimenguard.region.passthrough"},
    PermissionDefinition{CommandPermission::Membership, "dimenguard.region.membership"},
    PermissionDefinition{CommandPermission::Ownership, "dimenguard.region.ownership"},
    PermissionDefinition{CommandPermission::Teleport, "dimenguard.region.teleport"},
    PermissionDefinition{CommandPermission::Recovery, "dimenguard.recovery"},
};

}

std::span<const CommandPermission> commandPermissions()
{
    static constexpr auto values = [] {
        std::array<CommandPermission, definitions.size()> result{};
        for (std::size_t index = 0; index < definitions.size(); ++index) {
            result[index] = definitions[index].permission;
        }
        return result;
    }();
    return values;
}

std::string_view permissionName(CommandPermission permission)
{
    for (const auto &definition : definitions) {
        if (definition.permission == permission) {
            return definition.name;
        }
    }
    throw std::invalid_argument("Unknown command permission");
}

std::optional<CommandPermission> permissionForCommand(std::string_view path)
{
    if (path == "pos1" || path == "pos2" || path == "region select") {
        return CommandPermission::Selection;
    }
    if (path == "inspect") {
        return CommandPermission::Inspect;
    }
    if (path == "region create") {
        return CommandPermission::Create;
    }
    if (path == "region claim") {
        return CommandPermission::Claim;
    }
    if (path == "region delete") {
        return CommandPermission::Delete;
    }
    if (path == "region rename") {
        return CommandPermission::Rename;
    }
    if (path == "region redefine") {
        return CommandPermission::Redefine;
    }
    if (path == "region move") {
        return CommandPermission::Move;
    }
    if (path == "region list") {
        return CommandPermission::List;
    }
    if (path == "region info") {
        return CommandPermission::Info;
    }
    if (path == "region flags" || path == "flag") {
        return CommandPermission::Flags;
    }
    if (path == "region priority") {
        return CommandPermission::Priority;
    }
    if (path == "region set-parent") {
        return CommandPermission::Parent;
    }
    if (path == "region set-passthrough") {
        return CommandPermission::Passthrough;
    }
    if (path == "region set-flag" || path == "region unset-flag") {
        return CommandPermission::Flags;
    }
    if (path == "trust" || path == "untrust") {
        return CommandPermission::Membership;
    }
    if (path == "region set-owner") {
        return CommandPermission::Ownership;
    }
    if (path == "reload") {
        return CommandPermission::Recovery;
    }
    return std::nullopt;
}

std::string flagPermissionName(Flag flag)
{
    return std::format("dimenguard.region.flag.{}", flagName(flag));
}

std::string flagValuePermissionName(Flag flag, std::string_view value)
{
    return std::format("{}.{}", flagPermissionName(flag), value);
}

}
