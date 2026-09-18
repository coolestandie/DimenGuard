#include "dimenguard/adapter/context.h"
#include "dimenguard/command/context.h"
#include "dimenguard/command/flag_input.h"
#include "dimenguard/command/handlers.h"
#include "dimenguard/command/parse.h"
#include "dimenguard/presentation/flag_panel.h"
#include "dimenguard/presentation/panel.h"

#include <algorithm>
#include <endstone/player.h>
#include <endstone/server.h>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dimenguard {
namespace {

struct RegionTarget {
    DimensionKey dimension;
    endstone::Player *player = nullptr;
    std::span<const std::string> arguments;
};

RegionTarget resolveTarget(CommandContext &context, endstone::CommandSender &sender,
                           std::span<const std::string> arguments)
{
    if (const auto player = sender.as<endstone::Player>(); player != nullptr) {
        return {dimensionKey(*player->getDimension()), &*player, arguments};
    }
    requireArgument(arguments.size() >= 2, Message::ConsoleDimensionRequired);
    const auto &level = context.server().getLevel();
    requireArgument(level.getName() == arguments[0], Message::DimensionNotFound);
    const auto dimension = level.getDimension(endstone::DimensionId{arguments[1]});
    requireArgument(static_cast<bool>(dimension), Message::DimensionNotFound);
    return {dimensionKey(*dimension), nullptr, arguments.subspan(2)};
}

RegionKey regionKey(const RegionTarget &target, std::string_view name)
{
    return {target.dimension, std::string(name)};
}

void requirePlayer(const RegionTarget &target)
{
    requireArgument(target.player != nullptr, Message::PlayerOnly);
}

void sendOverlapWarning(CommandContext &context, endstone::CommandSender &sender,
                        const std::vector<const Region *> &overlaps)
{
    if (overlaps.empty()) {
        return;
    }
    std::string names;
    constexpr std::size_t max_names = 8;
    for (std::size_t index = 0; index < std::min(max_names, overlaps.size()); ++index) {
        if (!names.empty()) {
            names += ", ";
        }
        names += overlaps[index]->key.name;
    }
    if (overlaps.size() > max_names) {
        names += ", ...";
    }
    context.messages().send(sender, Message::OverlapWarning, overlaps.size(), names);
}

void listRegions(CommandContext &context, endstone::CommandSender &sender, const RegionTarget &target)
{
    requireArgument(target.arguments.size() <= 1);
    const auto page = target.arguments.empty() ? std::optional<int>{1} : parseInteger(target.arguments[0]);
    requireArgument(page.has_value() && *page >= 1, Message::InvalidPage);
    const auto regions = context.service().getRegions().inDimension(target.dimension);
    constexpr std::size_t page_size = 10;
    const auto slice = paginate(regions.size(), static_cast<std::size_t>(*page), page_size);
    requireArgument(slice.has_value(), Message::InvalidPage);
    auto &messages = context.messages();
    messages.send(sender, Message::List, target.dimension.dimension, regions.size(), *page, slice->page_count);
    for (auto i = slice->offset; i < slice->offset + slice->count; ++i) {
        messages.send(sender, Message::ListItem, regions[i]->key.name, regions[i]->priority);
    }
}

const Region &findRegion(CommandContext &context, const RegionTarget &target, std::string_view name)
{
    const auto *region = context.service().getRegions().find(regionKey(target, name));
    requireArgument(region != nullptr, Message::NotFound);
    return *region;
}

void showRegion(CommandContext &context, endstone::CommandSender &sender, const RegionTarget &target,
                std::string_view name)
{
    const auto &region = findRegion(context, target, name);
    const auto &bounds = region.bounds;
    auto &messages = context.messages();
    messages.send(sender, Message::Info, name, region.key.dimension.dimension, region.priority, bounds.min.x,
                  bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y, bounds.max.z, region.owner,
                  region.members.size());
    messages.send(sender, Message::RegionInfoKind, regionKindName(region.kind));
    messages.send(sender, Message::RegionInfoParent, region.parent ? *region.parent : "none");
    messages.send(sender, Message::RegionInfoPassthrough, stateName(region.passthrough));
    messages.sendLines(sender, renderRegionFlags(region, messages.getLocale(sender)));
}

void createRegion(CommandContext &context, endstone::CommandSender &sender, const RegionTarget &target,
                  std::string_view name)
{
    requirePlayer(target);
    const auto bounds = context.selections().get(target.player->getUniqueId().str(), target.dimension);
    requireArgument(bounds.has_value(), Message::SelectionRequired);
    const auto overlaps = context.service().getRegions().overlaps(target.dimension, *bounds);
    const auto id = target.player->getUniqueId().str();
    context.service().create({regionKey(target, name), *bounds, 0, id, {}, {}});
    context.messages().send(sender, Message::Created, name);
    sendOverlapWarning(context, sender, overlaps);
}

void deleteRegion(CommandContext &context, endstone::CommandSender &sender, const RegionTarget &target)
{
    requireArgument(!target.arguments.empty());
    const auto key = regionKey(target, target.arguments.front());
    const auto children = context.service().hasChildren(key);
    if (children) {
        requireArgument(target.arguments.size() == 2 && target.arguments[1] == "confirm", Message::DeleteConfirmation);
        context.service().erase(key, true);
        context.messages().send(sender, Message::DeletedCascade, key.name);
        return;
    }
    requireArgument(target.arguments.size() == 1);
    context.service().erase(key);
    context.messages().send(sender, Message::Deleted, key.name);
}

void renameRegion(CommandContext &context, endstone::CommandSender &sender, const RegionTarget &target)
{
    requireArgument(target.arguments.size() == 2);
    const auto key = regionKey(target, target.arguments[0]);
    context.service().rename(key, target.arguments[1]);
    context.messages().send(sender, Message::Renamed, key.name, target.arguments[1]);
}

void redefineRegion(CommandContext &context, endstone::CommandSender &sender, const RegionTarget &target)
{
    requirePlayer(target);
    requireArgument(target.arguments.size() == 1);
    const auto key = regionKey(target, target.arguments[0]);
    const auto bounds = context.selections().get(target.player->getUniqueId().str(), target.dimension);
    requireArgument(bounds.has_value(), Message::SelectionRequired);
    const auto overlaps = context.service().getRegions().overlaps(target.dimension, *bounds, key.name);
    context.service().setBounds(key, *bounds);
    context.messages().send(sender, Message::Redefined, key.name);
    sendOverlapWarning(context, sender, overlaps);
}

void moveRegion(CommandContext &context, endstone::CommandSender &sender, const RegionTarget &target)
{
    requireArgument(target.arguments.size() == 4);
    const auto x = parseInteger(target.arguments[1]);
    const auto y = parseInteger(target.arguments[2]);
    const auto z = parseInteger(target.arguments[3]);
    requireArgument(x && y && z, Message::InvalidOffset);
    const auto key = regionKey(target, target.arguments[0]);
    requireArgument(context.service().getRegions().find(key) != nullptr, Message::NotFound);
    context.service().move(key, {*x, *y, *z});
    const auto *after = context.service().getRegions().find(key);
    requireArgument(after != nullptr, Message::NotFound);
    const auto overlaps = context.service().getRegions().overlaps(target.dimension, after->bounds, key.name);
    context.messages().send(sender, Message::Moved, key.name, *x, *y, *z);
    sendOverlapWarning(context, sender, overlaps);
}

void setParent(CommandContext &context, endstone::CommandSender &sender, const RegionTarget &target)
{
    requireArgument(target.arguments.size() == 2);
    const auto key = regionKey(target, target.arguments[0]);
    const std::string_view parent = target.arguments[1];
    if (parent == "none" || parent == "-") {
        context.service().setParent(key, std::nullopt);
        context.messages().send(sender, Message::ParentCleared, key.name);
        return;
    }
    context.service().setParent(key, regionKey(target, parent));
    context.messages().send(sender, Message::ParentSet, key.name, parent);
}

void setPassthrough(CommandContext &context, endstone::CommandSender &sender, const RegionTarget &target)
{
    requireArgument(target.arguments.size() == 2);
    const auto state = parseState(target.arguments[1]);
    requireArgument(state.has_value(), Message::InvalidFlagValue);
    const auto key = regionKey(target, target.arguments[0]);
    context.service().setPassthrough(key, *state);
    context.messages().send(sender, Message::PassthroughSet, key.name, stateName(*state));
}

void setFlag(CommandContext &context, endstone::CommandSender &sender, const RegionTarget &target)
{
    requireArgument(target.arguments.size() == 3);
    const auto flag = parseFlag(target.arguments[1]);
    requireArgument(flag.has_value(), Message::InvalidFlag);
    const auto change = parseFlagChange(*flag, target.arguments[2]);
    requireArgument(change.has_value(), Message::InvalidFlagValue);
    const auto key = regionKey(target, target.arguments[0]);
    context.service().setFlagValue(key, *flag, change->value);
    if (!change->value) {
        context.messages().send(sender, Message::FlagCleared, key.name, target.arguments[1]);
    }
    else {
        context.messages().send(sender, Message::FlagSet, key.name, target.arguments[1],
                                displayFlagValue(*change->value));
    }
}

void unsetFlag(CommandContext &context, endstone::CommandSender &sender, const RegionTarget &target)
{
    requireArgument(target.arguments.size() == 2);
    const auto flag = parseFlag(target.arguments[1]);
    requireArgument(flag.has_value(), Message::InvalidFlag);
    const auto key = regionKey(target, target.arguments[0]);
    context.service().setFlagValue(key, *flag, std::nullopt);
    context.messages().send(sender, Message::FlagCleared, key.name, target.arguments[1]);
}

void selectRegion(CommandContext &context, endstone::CommandSender &sender, const RegionTarget &target)
{
    requirePlayer(target);
    requireArgument(target.arguments.size() == 1);
    const auto &region = findRegion(context, target, target.arguments[0]);
    requireArgument(region.kind == RegionKind::Cuboid, Message::InvalidRegionType);
    context.selections().setBounds(target.player->getUniqueId().str(), target.dimension, region.bounds);
    context.messages().send(sender, Message::RegionSelected, region.key.name);
}

std::string_view canonicalAction(std::string_view action)
{
    if (action == "define") {
        return "create";
    }
    if (action == "remove") {
        return "delete";
    }
    if (action == "set-priority") {
        return "priority";
    }
    return action;
}

}

void executeRegionCommand(CommandContext &context, endstone::CommandSender &sender, std::span<const std::string> args)
{
    requireArgument(!args.empty());
    const auto action = canonicalAction(args.front());
    const auto target = resolveTarget(context, sender, args.subspan(1));
    if (action == "list") {
        listRegions(context, sender, target);
    }
    else if (action == "create" || action == "claim") {
        requireArgument(target.arguments.size() == 1);
        createRegion(context, sender, target, target.arguments.front());
    }
    else if (action == "delete") {
        deleteRegion(context, sender, target);
    }
    else if (action == "rename") {
        renameRegion(context, sender, target);
    }
    else if (action == "redefine") {
        redefineRegion(context, sender, target);
    }
    else if (action == "move") {
        moveRegion(context, sender, target);
    }
    else if (action == "info") {
        requireArgument(target.arguments.size() == 1);
        showRegion(context, sender, target, target.arguments.front());
    }
    else if (action == "flags") {
        requireArgument(target.arguments.size() == 1);
        const auto &region = findRegion(context, target, target.arguments.front());
        context.messages().sendLines(sender, renderRegionFlags(region, context.messages().getLocale(sender)));
    }
    else if (action == "priority") {
        requireArgument(target.arguments.size() == 2);
        const auto priority = parseInteger(target.arguments[1]);
        requireArgument(priority.has_value(), Message::InvalidNumber);
        const auto key = regionKey(target, target.arguments[0]);
        context.service().setPriority(key, *priority);
        context.messages().send(sender, Message::PrioritySet, key.name, *priority);
    }
    else if (action == "set-parent") {
        setParent(context, sender, target);
    }
    else if (action == "set-passthrough") {
        setPassthrough(context, sender, target);
    }
    else if (action == "set-flag") {
        setFlag(context, sender, target);
    }
    else if (action == "unset-flag") {
        unsetFlag(context, sender, target);
    }
    else if (action == "select") {
        selectRegion(context, sender, target);
    }
    else {
        throw CommandError(Message::Usage);
    }
}

}
