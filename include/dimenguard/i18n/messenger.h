#pragma once

#include "dimenguard/i18n/translator.h"

#include <chrono>
#include <endstone/command/command_sender.h>
#include <endstone/player.h>
#include <span>
#include <string>
#include <unordered_map>

namespace dimenguard {

class Messenger {
public:
    [[nodiscard]] Locale getLocale(const endstone::CommandSender &sender) const;
    void setLocale(const endstone::Player &player, Locale locale);
    void forget(const endstone::Player &player);
    void deny(endstone::Player &player, Message message = Message::Denied);
    void sendHelp(endstone::CommandSender &sender) const;
    void sendLines(endstone::CommandSender &sender, std::span<const std::string> lines) const;

    template <typename... Args>
    void send(endstone::CommandSender &sender, Message message, Args &&...args) const
    {
        sender.sendMessage(
            endstone::Message{Theme::decorate(translate(message, getLocale(sender), std::forward<Args>(args)...))});
    }

private:
    std::unordered_map<std::string, Locale> locales_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_denial_;
};

}  // namespace dimenguard
