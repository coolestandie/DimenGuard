#include "dimenguard/presentation/messenger.h"

#include "dimenguard/presentation/help_panel.h"

namespace dimenguard {

Locale Messenger::getLocale(const endstone::CommandSender &sender) const
{
    const auto *player = sender.as<endstone::Player>();
    if (player == nullptr) {
        return Locale::English;
    }
    const auto found = locales_.find(player->getUniqueId().str());
    return found == locales_.end() ? parseLocale(player->getLocale()) : found->second;
}

void Messenger::setLocale(const endstone::Player &player, Locale locale)
{
    locales_[player.getUniqueId().str()] = locale;
}

void Messenger::forget(const endstone::Player &player)
{
    const auto id = player.getUniqueId().str();
    locales_.erase(id);
    last_denial_.erase(id);
}

void Messenger::deny(endstone::Player &player, Message message)
{
    denyText(player, messageText(message, getLocale(player)));
}

void Messenger::denyText(endstone::Player &player, std::string_view text)
{
    const auto now = std::chrono::steady_clock::now();
    const auto id = player.getUniqueId().str();
    const auto found = last_denial_.find(id);
    if (found != last_denial_.end() && now - found->second < std::chrono::seconds(1)) {
        return;
    }
    last_denial_[id] = now;
    player.sendMessage(endstone::Message{Theme::decorate(text)});
}

void Messenger::sendHelp(endstone::CommandSender &sender) const
{
    sendLines(sender, renderHelp(getLocale(sender), sender.hasPermission("dimenguard.command")));
}

void Messenger::sendLines(endstone::CommandSender &sender, std::span<const std::string> lines) const
{
    for (const auto &line : lines) {
        sender.sendMessage(endstone::Message{line});
    }
}

}
