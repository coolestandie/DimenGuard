#include "dimenguard/command/catalog.h"
#include "dimenguard/command/parse.h"
#include "dimenguard/i18n/translator.h"

#include <array>
#include <gtest/gtest.h>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace dimenguard {
namespace {

std::string plainText(std::string_view formatted)
{
    std::string plain;
    for (std::size_t index = 0; index < formatted.size();) {
        if (formatted.substr(index).starts_with("\xc2\xa7") && index + 2 < formatted.size()) {
            index += 3;
        }
        else {
            plain += formatted[index++];
        }
    }
    return plain;
}

std::string plainHelp(const std::vector<std::string> &lines)
{
    std::string text;
    for (const auto &line : lines) {
        text += plainText(line) + '\n';
    }
    return text;
}

}  // namespace

TEST(Translation, EveryMessageFormatsInBothLocales)
{
    const std::string value = "sample";
    for (int i = 0; i < static_cast<int>(Message::Count); ++i) {
        for (const auto locale : {Locale::English, Locale::Spanish}) {
            const auto message = static_cast<Message>(i);
            EXPECT_FALSE(messageText(message, locale).empty());
            EXPECT_NO_THROW(static_cast<void>(translate(message, locale, value, value, value, value, value, value,
                                                        value, value, value, value, value)));
        }
        const auto en = messageText(static_cast<Message>(i), Locale::English);
        const auto es = messageText(static_cast<Message>(i), Locale::Spanish);
        for (int argument = 0; argument <= 10; ++argument) {
            const auto placeholder = std::format("{{{}}}", argument);
            EXPECT_EQ(en.find(placeholder) != en.npos, es.find(placeholder) != es.npos);
        }
    }
}

TEST(Translation, LocaleUsesClientLanguageAndFallsBackToEnglish)
{
    EXPECT_EQ(parseLocale("es_MX"), Locale::Spanish);
    EXPECT_EQ(parseLocale("ES-es"), Locale::Spanish);
    EXPECT_EQ(parseLocale("es"), Locale::Spanish);
    EXPECT_EQ(parseLocale("estonian"), Locale::English);
    EXPECT_EQ(parseLocale("de_DE"), Locale::English);
    EXPECT_EQ(parseLocale(""), Locale::English);
    EXPECT_NE(translate(Message::Denied, Locale::English), translate(Message::Denied, Locale::Spanish));
}

TEST(Translation, ThemeMatchesUnbracketedReferenceAndResetsFormatting)
{
    const auto result = Theme::decorate("Test");
    EXPECT_EQ(plainText(result), "DimenGuard > Test");
    EXPECT_TRUE(result.starts_with(std::format("{}Dimen{}Guard", Theme::Amethyst, Theme::LightGray)));
    EXPECT_NE(result.find(std::format("{} > {}Test", Theme::DarkGray, Theme::White)), result.npos);
    EXPECT_TRUE(result.ends_with(Theme::Reset));
}

TEST(HelpPanel, HasLocalizedSectionsAndOneBrandedTitle)
{
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        const auto lines = renderHelp(locale, true);
        ASSERT_GE(lines.size(), 3);
        EXPECT_EQ(lines.front(), lines.back());
        EXPECT_EQ(lines[1], Theme::decorate(messageText(Message::Help, locale)));
        const auto text = plainHelp(lines);
        for (const auto section :
             {Message::HelpSelection, Message::HelpRegions, Message::HelpProtection, Message::HelpGeneral}) {
            EXPECT_NE(text.find(messageText(section, locale)), text.npos);
        }
        const auto title = text.find("DimenGuard");
        ASSERT_NE(title, text.npos);
        EXPECT_EQ(text.find("DimenGuard", title + 1), text.npos);
        EXPECT_EQ(text.find("UUID"), text.npos);
    }
}

TEST(HelpPanel, HidesAdministrativeCommandsWithoutPermission)
{
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        const auto text = plainHelp(renderHelp(locale, false));
        EXPECT_NE(text.find("/dg help"), text.npos);
        EXPECT_NE(text.find("/dg language"), text.npos);
        for (const auto command : {"/dg pos", "/dg region", "/dg flag", "/dg trust", "/dg untrust", "/dg reload"}) {
            EXPECT_EQ(text.find(command), text.npos) << command;
        }
        for (const auto section : {Message::HelpSelection, Message::HelpRegions, Message::HelpProtection}) {
            EXPECT_EQ(text.find(messageText(section, locale)), text.npos);
        }
    }
}

TEST(HelpPanel, UsesRegisteredSyntaxAndTranslatedDescriptions)
{
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        const auto text = plainHelp(renderHelp(locale, true));
        for (const auto &command : commandCatalog()) {
            const auto usage = helpUsage(command);
            EXPECT_NE(text.find(usage), text.npos) << usage;
            EXPECT_NE(text.find(messageText(command.description, locale)), text.npos) << usage;
        }
    }
}

TEST(HelpPanel, StaysBoundedAndResetsEveryLine)
{
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        for (const bool can_manage : {false, true}) {
            const auto lines = renderHelp(locale, can_manage);
            EXPECT_LE(lines.size(), 28);
            for (const auto &line : lines) {
                EXPECT_EQ(line.find('\n'), line.npos);
                EXPECT_TRUE(line.ends_with(Theme::Reset));
                EXPECT_LE(plainText(line).size(), 110);
            }
        }
    }
}

TEST(FlagSupport, EnumeratesUniqueNamesThatRoundTripThroughParsers)
{
    std::set<std::string_view> flags;
    ASSERT_EQ(supportedFlags().size(), 4);
    for (const auto flag : supportedFlags()) {
        EXPECT_TRUE(flags.insert(flagName(flag)).second);
        EXPECT_EQ(parseFlag(flagName(flag)), flag);
    }
    EXPECT_EQ(flags, (std::set<std::string_view>{"build", "interact", "container-access", "pvp"}));
    std::set<std::string_view> states;
    ASSERT_EQ(supportedFlagStates().size(), 3);
    for (const auto state : supportedFlagStates()) {
        EXPECT_TRUE(states.insert(stateName(state)).second);
        EXPECT_EQ(parseState(stateName(state)), state);
    }
    EXPECT_EQ(states, (std::set<std::string_view>{"allow", "deny", "inherit"}));
}

TEST(FlagPresentation, CatalogShowsEveryFlagStatesAndLocalizedUsage)
{
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        const auto lines = renderFlagCatalog(locale);
        const auto text = plainHelp(lines);
        EXPECT_EQ(lines.front(), lines.back());
        EXPECT_NE(text.find(messageText(Message::FlagsAvailable, locale)), text.npos);
        for (const auto flag : supportedFlags()) {
            EXPECT_NE(text.find(std::format("{} / ", flagName(flag))), text.npos);
        }
        for (const auto state : supportedFlagStates()) {
            EXPECT_NE(text.find(stateName(state)), text.npos);
        }
        EXPECT_NE(text.find("/dg flag spawn pvp deny"), text.npos);
        EXPECT_NE(text.find(locale == Locale::English ? "Usage:" : "Uso:"), text.npos);
        for (const auto description : {Message::FlagBuildDescription, Message::FlagInteractDescription,
                                       Message::FlagContainerDescription, Message::FlagPvpDescription}) {
            EXPECT_NE(text.find(messageText(description, locale)), text.npos);
            EXPECT_NE(messageText(description, Locale::English), messageText(description, Locale::Spanish));
        }
        for (const auto &line : lines) {
            EXPECT_TRUE(line.ends_with(Theme::Reset));
            EXPECT_EQ(line.find('\n'), line.npos);
        }
    }
}

TEST(FlagPresentation, RegionQueriesShowStoredAndInheritedStatesWithoutMutation)
{
    Region region;
    region.key.name = "spawn";
    region.flags = {{Flag::Build, FlagState::Allow}, {Flag::Pvp, FlagState::Deny}};
    const auto before = region.flags;
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        const auto text = plainHelp(renderRegionFlags(region, locale));
        EXPECT_NE(text.find("spawn"), text.npos);
        EXPECT_NE(text.find("build: allow"), text.npos);
        EXPECT_NE(text.find("pvp: deny"), text.npos);
        EXPECT_NE(text.find("interact: inherit"), text.npos);
        EXPECT_NE(text.find("container-access: inherit"), text.npos);
        EXPECT_NE(text.find(messageText(Message::FlagInheritance, locale)), text.npos);
        EXPECT_EQ(region.flags, before);
    }
}

TEST(FlagPresentation, SingleFlagQueryShowsOnlyRequestedStoredValue)
{
    Region region;
    region.key.name = "spawn";
    region.flags = {{Flag::Build, FlagState::Allow}, {Flag::Pvp, FlagState::Deny}};
    const auto before = region.flags;
    const auto text = plainHelp(renderRegionFlags(region, Locale::English, Flag::Pvp));
    EXPECT_NE(text.find("pvp: deny"), text.npos);
    EXPECT_EQ(text.find("build:"), text.npos);
    EXPECT_EQ(text.find("interact:"), text.npos);
    EXPECT_EQ(text.find("container-access:"), text.npos);
    EXPECT_EQ(region.flags, before);
    EXPECT_THROW(static_cast<void>(renderRegionFlags(region, Locale::English, static_cast<Flag>(999))),
                 std::invalid_argument);
}

TEST(CommandParsing, RejectsPartialAndOverflowingIntegers)
{
    EXPECT_EQ(parseInteger("-2147483648"), -2147483647 - 1);
    EXPECT_EQ(parseInteger("2147483647"), 2147483647);
    for (const auto text : {"", "12foo", "2147483648", "-2147483649", " 2", "1.5"}) {
        EXPECT_FALSE(parseInteger(text));
    }
}

}  // namespace dimenguard
