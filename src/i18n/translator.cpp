#include "dimenguard/i18n/translator.h"

#include "dimenguard/command/catalog.h"

#include <array>
#include <cctype>
#include <stdexcept>

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

void appendHelpSection(std::vector<std::string> &lines, Locale locale, bool can_manage, CommandSection section,
                       Message heading)
{
    bool started = false;
    for (const auto &entry : commandCatalog()) {
        if (entry.section != section || (entry.requires_admin && !can_manage)) {
            continue;
        }
        if (!started) {
            lines.emplace_back(Theme::Reset);
            lines.push_back(std::format("  {}{}{}", Theme::Amethyst, messageText(heading, locale), Theme::Reset));
            started = true;
        }
        const auto usage = helpUsage(entry);
        const auto argument_start = usage.find_first_of("<[");
        const auto command = std::string_view(usage).substr(0, argument_start);
        const auto arguments =
            argument_start == usage.npos ? std::string_view{} : std::string_view(usage).substr(argument_start);
        lines.push_back(std::format("  {}{}{}{}{} / {}{}{}", Theme::White, command, Theme::LightGray, arguments,
                                    Theme::DarkGray, Theme::LightGray, messageText(entry.description, locale),
                                    Theme::Reset));
    }
}
}  // namespace

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

std::vector<std::string> renderHelp(Locale locale, bool can_manage)
{
    const auto rule = std::format("{}------------------------------------------{}", Theme::DarkGray, Theme::Reset);
    std::vector<std::string> lines{rule, Theme::decorate(messageText(Message::Help, locale)), rule};
    appendHelpSection(lines, locale, can_manage, CommandSection::Selection, Message::HelpSelection);
    appendHelpSection(lines, locale, can_manage, CommandSection::Regions, Message::HelpRegions);
    appendHelpSection(lines, locale, can_manage, CommandSection::Protection, Message::HelpProtection);
    appendHelpSection(lines, locale, can_manage, CommandSection::General, Message::HelpGeneral);
    lines.push_back(rule);
    return lines;
}

std::string Theme::identity()
{
    return std::format("{}Dimen{}Guard", Amethyst, LightGray);
}

std::string Theme::decorate(std::string_view text)
{
    return std::format("{}{} > {}{}{}", identity(), DarkGray, White, text, Reset);
}

}  // namespace dimenguard
