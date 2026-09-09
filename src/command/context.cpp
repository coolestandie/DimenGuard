#include "dimenguard/command/context.h"

#include "dimenguard/adapter/context.h"
#include "dimenguard/command/parse.h"
#include "dimenguard/plugin.h"
namespace dimenguard {
void requireArgument(bool condition, Message message)
{
    if (!condition) {
        throw CommandError(message);
    }
}
CommandContext::CommandContext(DimenGuardPlugin &plugin) : plugin_(plugin) {}
Messenger &CommandContext::messages() const
{
    return plugin_.getMessenger();
}
RegionService &CommandContext::service() const
{
    const auto service = plugin_.getService();
    requireArgument(service != nullptr, Message::NotReady);
    return *service;
}
SelectionManager &CommandContext::selections() const
{
    return plugin_.getSelections();
}
endstone::Player &CommandContext::player(endstone::CommandSender &sender) const
{
    const auto player = sender.as<endstone::Player>();
    requireArgument(player != nullptr, Message::PlayerOnly);
    return *player;
}
const Region &CommandContext::region(const endstone::Player &player, const std::string &name) const
{
    const auto found = service().getRegions().find({dimensionKey(*player.getDimension()), name});
    requireArgument(found != nullptr, Message::NotFound);
    return *found;
}
endstone::NotNull<endstone::Player> CommandContext::resolvePlayer(std::string_view argument) const
{
    const auto name = parsePlayerName(argument);
    requireArgument(name.has_value(), Message::PlayerNotFound);
    const auto player = plugin_.getServer().getPlayer(std::string(*name));
    requireArgument(player && playerNamesMatch(player->getName(), *name), Message::PlayerNotFound);
    return endstone::NotNull<endstone::Player>{player};
}
void CommandContext::reload() const
{
    plugin_.reloadRegions();
}
}
