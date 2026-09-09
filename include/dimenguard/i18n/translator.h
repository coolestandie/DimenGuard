#pragma once

#include <format>
#include <string>
#include <string_view>

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

template <typename... Args>
[[nodiscard]] std::string translate(Message message, Locale locale, Args &&...args)
{
    return std::vformat(messageText(message, locale), std::make_format_args(args...));
}

}
