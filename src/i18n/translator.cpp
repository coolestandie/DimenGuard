#include "dimenguard/i18n/translator.h"

#include "dimenguard/command/catalog.h"

#include <algorithm>
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

std::string panelRule()
{
    return std::format("{}------------------------------------------{}", Theme::DarkGray, Theme::Reset);
}

Message flagDescription(Flag flag)
{
    switch (flag) {
    case Flag::Build:
        return Message::FlagBuildDescription;
    case Flag::Interact:
        return Message::FlagInteractDescription;
    case Flag::ContainerAccess:
        return Message::FlagContainerDescription;
    case Flag::Pvp:
        return Message::FlagPvpDescription;
    }
    throw std::invalid_argument("A supported flag has no description");
}

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
    const auto rule = panelRule();
    std::vector<std::string> lines{rule, Theme::decorate(messageText(Message::Help, locale)), rule};
    appendHelpSection(lines, locale, can_manage, CommandSection::Selection, Message::HelpSelection);
    appendHelpSection(lines, locale, can_manage, CommandSection::Regions, Message::HelpRegions);
    appendHelpSection(lines, locale, can_manage, CommandSection::Protection, Message::HelpProtection);
    appendHelpSection(lines, locale, can_manage, CommandSection::General, Message::HelpGeneral);
    lines.push_back(rule);
    return lines;
}

std::vector<std::string> renderFlagCatalog(Locale locale)
{
    const auto rule = panelRule();
    std::vector<std::string> lines{rule, Theme::decorate(messageText(Message::FlagsAvailable, locale)), rule};
    for (const auto flag : supportedFlags()) {
        lines.push_back(std::format("  {}{}{} / {}{}{}", Theme::Amethyst, flagName(flag), Theme::DarkGray,
                                    Theme::LightGray, messageText(flagDescription(flag), locale), Theme::Reset));
    }
    std::string states;
    for (const auto state : supportedFlagStates()) {
        if (!states.empty()) {
            states += ", ";
        }
        states += stateName(state);
    }
    lines.push_back(std::format("{}{}{}", Theme::White, translate(Message::FlagStates, locale, states), Theme::Reset));
    const auto commands = commandCatalog();
    const auto command = std::ranges::find(commands, std::string_view{"flag"}, &CommandSpec::path);
    if (command == commands.end()) {
        throw std::logic_error("The flag command is missing from the command catalog");
    }
    lines.push_back(std::format("{}{}{}", Theme::LightGray, translate(Message::FlagSyntax, locale, helpUsage(*command)),
                                Theme::Reset));
    lines.push_back(std::format("{}{}{}", Theme::LightGray, messageText(Message::FlagExample, locale), Theme::Reset));
    lines.push_back(rule);
    return lines;
}

std::vector<std::string> renderRegionFlags(const Region &region, Locale locale, std::optional<Flag> selected)
{
    if (selected) {
        static_cast<void>(flagName(*selected));
    }
    std::vector<std::string> lines{Theme::decorate(translate(Message::RegionFlags, locale, region.key.name))};
    for (const auto flag : supportedFlags()) {
        if (selected && flag != *selected) {
            continue;
        }
        const auto found = region.flags.find(flag);
        const auto state = found == region.flags.end() ? FlagState::Inherit : found->second;
        const auto value = std::format("{}{}", Theme::LightGray, stateName(state));
        lines.push_back(std::format("  {}{}{}", Theme::White,
                                    translate(Message::FlagInfo, locale, flagName(flag), value), Theme::Reset));
    }
    lines.push_back(
        std::format("{}{}{}", Theme::LightGray, messageText(Message::FlagInheritance, locale), Theme::Reset));
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
