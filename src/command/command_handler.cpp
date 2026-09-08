#include "dimenguard/command/command_handler.h"

#include "dimenguard/adapter/context.h"
#include "dimenguard/command/parse.h"
#include "dimenguard/plugin.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dimenguard {
namespace {

class CommandError : public std::exception {
public:
    explicit CommandError(Message message) : message_(message) {}
    [[nodiscard]] Message getMessage() const { return message_; }

private:
    Message message_;
};

void require(bool condition, Message message = Message::Usage)
{
    if (!condition) {
        throw CommandError(message);
    }
}

endstone::Player &requirePlayer(endstone::CommandSender &sender)
{
    auto *player = sender.as<endstone::Player>();
    require(player != nullptr, Message::PlayerOnly);
    return *player;
}

Message serviceMessage(ServiceErrorCode code)
{
    switch (code) {
    case ServiceErrorCode::NotFound:
        return Message::NotFound;
    case ServiceErrorCode::Exists:
        return Message::Exists;
    case ServiceErrorCode::InvalidName:
        return Message::InvalidName;
    case ServiceErrorCode::LimitReached:
        return Message::LimitReached;
    }
    return Message::Failed;
}

}  // namespace

CommandHandler::CommandHandler(DimenGuardPlugin &plugin) : plugin_(plugin) {}

void CommandHandler::execute(endstone::CommandSender &sender, std::span<const std::string> args)
{
    try {
        dispatch(sender, args);
    }
    catch (const CommandError &error) {
        plugin_.getMessenger().send(sender, error.getMessage());
    }
    catch (const ServiceError &error) {
        plugin_.getMessenger().send(sender, serviceMessage(error.getCode()));
    }
    catch (const std::exception &error) {
        plugin_.getLogger().error("Command failed: {}", error.what());
        plugin_.getMessenger().send(sender, Message::Failed);
    }
}

void CommandHandler::dispatch(endstone::CommandSender &sender, std::span<const std::string> args)
{
    auto &messages = plugin_.getMessenger();
    if (args.empty() || args[0] == "help") {
        require(args.size() <= 1);
        messages.send(sender, Message::Help);
        return;
    }
    if (args[0] == "language") {
        require(args.size() == 2 && (args[1] == "en" || args[1] == "es"));
        auto &player = requirePlayer(sender);
        messages.setLocale(player, parseLocale(args[1]));
        messages.send(player, Message::LanguageSet, args[1]);
        return;
    }
    require(sender.hasPermission("dimenguard.command"), Message::NoPermission);
    if (args[0] == "reload") {
        require(args.size() == 1);
        plugin_.reloadRegions();
        messages.send(sender, Message::Reloaded, plugin_.getService()->getRegions().getAll().size());
        return;
    }
    require(plugin_.getService() != nullptr, Message::NotReady);
    auto &player = requirePlayer(sender);
    auto &service = *plugin_.getService();
    const auto dimension = dimensionKey(*player.getDimension());
    if (args[0] == "pos1" || args[0] == "pos2") {
        require(args.size() == 1);
        const bool first = args[0] == "pos1";
        const auto position = blockPosition(player.getLocation());
        plugin_.getSelections().set(player.getUniqueId().str(), dimension, position,
                                    first ? SelectionCorner::First : SelectionCorner::Second);
        messages.send(player, Message::Selected, first ? 1 : 2, position.x, position.y, position.z,
                      dimension.dimension);
    }
    else if (args[0] == "region") {
        region(player, args.subspan(1));
    }
    else if (args[0] == "flag") {
        require(args.size() == 4);
        const auto flag = parseFlag(args[2]);
        const auto state = parseState(args[3]);
        require(flag.has_value() && state.has_value(), Message::InvalidFlag);
        service.setFlag({dimension, args[1]}, *flag, *state);
        messages.send(player, Message::FlagSet, args[1], args[2], args[3]);
    }
    else if (args[0] == "trust" || args[0] == "untrust") {
        require(args.size() == 3);
        const auto id = resolveIdentity(args[2]);
        service.setMember({dimension, args[1]}, id, args[0] == "trust");
        messages.send(player, Message::MemberSet, args[1], id);
    }
    else {
        throw CommandError(Message::Usage);
    }
}

void CommandHandler::region(endstone::Player &player, std::span<const std::string> args)
{
    require(!args.empty());
    if (args[0] == "list") {
        listRegions(player, args.subspan(1));
        return;
    }
    require(args.size() >= 2);
    const auto dimension = dimensionKey(*player.getDimension());
    const RegionKey key{dimension, args[1]};
    auto &service = *plugin_.getService();
    auto &messages = plugin_.getMessenger();
    if (args[0] == "create") {
        require(args.size() == 2);
        const auto id = player.getUniqueId().str();
        const auto bounds = plugin_.getSelections().get(id, dimension);
        require(bounds.has_value(), Message::SelectionRequired);
        service.create({key, *bounds, 0, id, {}, {}});
        messages.send(player, Message::Created, key.name);
    }
    else if (args[0] == "delete") {
        require(args.size() == 2);
        service.erase(key);
        messages.send(player, Message::Deleted, key.name);
    }
    else if (args[0] == "rename") {
        require(args.size() == 3);
        service.rename(key, args[2]);
        messages.send(player, Message::Renamed, key.name, args[2]);
    }
    else if (args[0] == "priority") {
        require(args.size() == 3);
        const auto priority = parseInteger(args[2]);
        require(priority.has_value(), Message::InvalidNumber);
        service.setPriority(key, *priority);
        messages.send(player, Message::PrioritySet, key.name, *priority);
    }
    else if (args[0] == "info") {
        require(args.size() == 2);
        showRegion(player, key.name);
    }
    else {
        throw CommandError(Message::Usage);
    }
}

void CommandHandler::listRegions(endstone::Player &player, std::span<const std::string> args)
{
    require(args.size() <= 1);
    const auto page = args.empty() ? std::optional<int>{1} : parseInteger(args[0]);
    require(page.has_value() && *page >= 1, Message::InvalidPage);
    const auto dimension = dimensionKey(*player.getDimension());
    std::vector<const Region *> regions;
    for (const auto &region : plugin_.getService()->getRegions().getAll()) {
        if (region.key.dimension == dimension) {
            regions.push_back(&region);
        }
    }
    std::ranges::sort(regions, [](const Region *a, const Region *b) { return a->key.name < b->key.name; });
    constexpr std::size_t page_size = 10;
    const auto pages = std::max(std::size_t{1}, (regions.size() + page_size - 1) / page_size);
    require(static_cast<std::size_t>(*page) <= pages, Message::InvalidPage);
    auto &messages = plugin_.getMessenger();
    messages.send(player, Message::List, dimension.dimension, regions.size(), *page, pages);
    const auto start = static_cast<std::size_t>(*page - 1) * page_size;
    for (auto i = start; i < std::min(start + page_size, regions.size()); ++i) {
        messages.send(player, Message::ListItem, regions[i]->key.name, regions[i]->priority);
    }
}

void CommandHandler::showRegion(endstone::Player &player, const std::string &name)
{
    const auto *region = plugin_.getService()->getRegions().find({dimensionKey(*player.getDimension()), name});
    require(region != nullptr, Message::NotFound);
    const auto &bounds = region->bounds;
    auto &messages = plugin_.getMessenger();
    messages.send(player, Message::Info, name, region->key.dimension.dimension, region->priority, bounds.min.x,
                  bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y, bounds.max.z, region->owner,
                  region->members.size());
    for (const auto flag : {Flag::Build, Flag::Interact, Flag::ContainerAccess, Flag::Pvp}) {
        const auto found = region->flags.find(flag);
        messages.send(player, Message::FlagInfo, flagName(flag),
                      stateName(found == region->flags.end() ? FlagState::Inherit : found->second));
    }
}

std::string CommandHandler::resolveIdentity(const std::string &name) const
{
    if (isCanonicalUuid(name)) {
        return name;
    }
    const auto player = plugin_.getServer().getPlayer(name);
    require(player && player->getName() == name, Message::PlayerNotFound);
    return player->getUniqueId().str();
}

}  // namespace dimenguard
