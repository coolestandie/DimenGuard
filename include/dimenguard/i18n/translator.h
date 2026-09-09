#pragma once

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dimenguard {

enum class Locale {
    English,
    Spanish
};
enum class Message {
#define DG_MESSAGE(Key, English, Spanish) Key,
#include "dimenguard/i18n/messages.inc"
#undef DG_MESSAGE
    Count
};

[[nodiscard]] Locale parseLocale(std::string_view locale);
[[nodiscard]] std::string_view messageText(Message message, Locale locale);
[[nodiscard]] std::vector<std::string> renderHelp(Locale locale, bool can_manage);

template <typename... Args>
[[nodiscard]] std::string translate(Message message, Locale locale, Args &&...args)
{
    return std::vformat(messageText(message, locale), std::make_format_args(args...));
}

struct Theme {
    static constexpr std::string_view Amethyst = "\xc2\xa7u";
    static constexpr std::string_view White = "\xc2\xa7"
                                              "f";
    static constexpr std::string_view LightGray = "\xc2\xa7"
                                                  "7";
    static constexpr std::string_view Muted = "\xc2\xa7j";
    static constexpr std::string_view DarkGray = "\xc2\xa7"
                                                 "8";
    static constexpr std::string_view Reset = "\xc2\xa7r";

    [[nodiscard]] static std::string identity();
    [[nodiscard]] static std::string decorate(std::string_view text);
};

}  // namespace dimenguard
