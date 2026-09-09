#include "dimenguard/adapter/context.h"
#include "dimenguard/command/context.h"
#include "dimenguard/command/handlers.h"
#include "dimenguard/command/parse.h"
#include "dimenguard/presentation/flag_panel.h"

#include <algorithm>
namespace dimenguard {
namespace {
void listRegions(CommandContext &context, endstone::Player &player, std::span<const std::string> args)
{
    requireArgument(args.size() <= 1);
    const auto page = args.empty() ? std::optional<int>{1} : parseInteger(args[0]);
    requireArgument(page.has_value() && *page >= 1, Message::InvalidPage);
    const auto dimension = dimensionKey(*player.getDimension());
    const auto regions = context.service().getRegions().inDimension(dimension);
    constexpr std::size_t page_size = 10;
    const auto pages = std::max(std::size_t{1}, (regions.size() + page_size - 1) / page_size);
    requireArgument(static_cast<std::size_t>(*page) <= pages, Message::InvalidPage);
    auto &messages = context.messages();
    messages.send(player, Message::List, dimension.dimension, regions.size(), *page, pages);
    const auto start = static_cast<std::size_t>(*page - 1) * page_size;
    for (auto i = start; i < std::min(start + page_size, regions.size()); ++i) {
        messages.send(player, Message::ListItem, regions[i]->key.name, regions[i]->priority);
    }
}
void showRegion(CommandContext &context, endstone::Player &player, const std::string &name)
{
    const auto &region = context.region(player, name);
    const auto &bounds = region.bounds;
    auto &messages = context.messages();
    messages.send(player, Message::Info, name, region.key.dimension.dimension, region.priority, bounds.min.x,
                  bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y, bounds.max.z, region.owner,
                  region.members.size());
    messages.sendLines(player, renderRegionFlags(region, messages.getLocale(player)));
}
}
void executeRegionCommand(CommandContext &context, endstone::CommandSender &sender, std::span<const std::string> args)
{
    static_cast<void>(context.service());
    auto &player = context.player(sender);
    requireArgument(!args.empty());
    if (args[0] == "list") {
        listRegions(context, player, args.subspan(1));
        return;
    }
    requireArgument(args.size() >= 2);
    const auto dimension = dimensionKey(*player.getDimension());
    const RegionKey key{dimension, args[1]};
    auto &service = context.service();
    auto &messages = context.messages();
    if (args[0] == "create") {
        requireArgument(args.size() == 2);
        const auto id = player.getUniqueId().str();
        const auto bounds = context.selections().get(id, dimension);
        requireArgument(bounds.has_value(), Message::SelectionRequired);
        service.create({key, *bounds, 0, id, {}, {}});
        messages.send(player, Message::Created, key.name);
    }
    else if (args[0] == "delete") {
        requireArgument(args.size() == 2);
        service.erase(key);
        messages.send(player, Message::Deleted, key.name);
    }
    else if (args[0] == "rename") {
        requireArgument(args.size() == 3);
        service.rename(key, args[2]);
        messages.send(player, Message::Renamed, key.name, args[2]);
    }
    else if (args[0] == "priority") {
        requireArgument(args.size() == 3);
        const auto priority = parseInteger(args[2]);
        requireArgument(priority.has_value(), Message::InvalidNumber);
        service.setPriority(key, *priority);
        messages.send(player, Message::PrioritySet, key.name, *priority);
    }
    else if (args[0] == "info") {
        requireArgument(args.size() == 2);
        showRegion(context, player, key.name);
    }
    else {
        throw CommandError(Message::Usage);
    }
}
}
