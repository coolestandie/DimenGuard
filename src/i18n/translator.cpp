#include "dimenguard/i18n/translator.h"

#include <array>
#include <cctype>

namespace dimenguard {
namespace {

struct Translation {
    std::string_view english;
    std::string_view spanish;
};

constexpr std::array catalog{
#define DG_MESSAGE(Key, English, Spanish) Translation{English, Spanish},
#include "dimenguard/i18n/messages.inc"
#undef DG_MESSAGE
};
static_assert(catalog.size() == static_cast<std::size_t>(Message::Count));

}

Locale parseLocale(std::string_view locale)
{
    if (locale.size() >= 2 && std::tolower(static_cast<unsigned char>(locale[0])) == 'e' &&
        std::tolower(static_cast<unsigned char>(locale[1])) == 's' &&
        (locale.size() == 2 || locale[2] == '_' || locale[2] == '-')) {
        return Locale::Spanish;
    }
    return Locale::English;
}

std::string_view messageText(Message message, Locale locale)
{
    const auto &entry = catalog.at(static_cast<std::size_t>(message));
    return locale == Locale::Spanish ? entry.spanish : entry.english;
}

}
