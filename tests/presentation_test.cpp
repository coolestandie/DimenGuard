#include "dimenguard/command/catalog.h"
#include "dimenguard/command/parse.h"
#include "dimenguard/i18n/translator.h"
#include "dimenguard/presentation/flag_panel.h"
#include "dimenguard/presentation/help_panel.h"
#include "dimenguard/presentation/panel.h"
#include "dimenguard/presentation/theme.h"

#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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

}

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

TEST(PanelFormatting, SharedFramedRowsKeepExactColorsAndSpacing)
{
    PanelBuilder panel("Title");
    panel.heading("Section");
    panel.entry("label", "description");
    panel.line("value", Theme::White);
    const auto rule = std::format("{}------------------------------------------{}", Theme::DarkGray, Theme::Reset);
    EXPECT_EQ(std::move(panel).finish(), (std::vector<std::string>{
                                             rule,
                                             Theme::decorate("Title"),
                                             rule,
                                             std::string(Theme::Reset),
                                             std::format("  {}Section{}", Theme::Amethyst, Theme::Reset),
                                             std::format("  {}label{} / {}description{}", Theme::Amethyst,
                                                         Theme::DarkGray, Theme::LightGray, Theme::Reset),
                                             std::format("{}value{}", Theme::White, Theme::Reset),
                                             rule,
                                         }));
}

TEST(PanelFormatting, CompactRowsDoNotAddFramingAndKeepIndentation)
{
    PanelBuilder panel("Title", PanelStyle::Compact);
    panel.line("body", Theme::White, "  ");
    panel.line("footer");
    EXPECT_EQ(std::move(panel).finish(),
              (std::vector<std::string>{Theme::decorate("Title"), std::format("  {}body{}", Theme::White, Theme::Reset),
                                        std::format("{}footer{}", Theme::LightGray, Theme::Reset)}));
}

TEST(PanelPagination, SlicesFirstMiddleAndLastPages)
{
    const auto first = paginate(25, 1, 6);
    const auto middle = paginate(25, 3, 6);
    const auto last = paginate(25, 5, 6);
    ASSERT_TRUE(first);
    ASSERT_TRUE(middle);
    ASSERT_TRUE(last);
    EXPECT_EQ(first->offset, 0);
    EXPECT_EQ(first->count, 6);
    EXPECT_EQ(first->page, 1);
    EXPECT_EQ(first->page_count, 5);
    EXPECT_EQ(middle->offset, 12);
    EXPECT_EQ(middle->count, 6);
    EXPECT_EQ(middle->page, 3);
    EXPECT_EQ(middle->page_count, 5);
    EXPECT_EQ(last->offset, 24);
    EXPECT_EQ(last->count, 1);
    EXPECT_EQ(last->page, 5);
    EXPECT_EQ(last->page_count, 5);
}

TEST(PanelPagination, RejectsInvalidPagesAndPageSizes)
{
    EXPECT_FALSE(paginate(25, 0, 6));
    EXPECT_FALSE(paginate(25, 6, 6));
    EXPECT_FALSE(paginate(25, 1, 0));
    EXPECT_FALSE(paginate(25, std::numeric_limits<std::size_t>::max(), 6));
}

TEST(PanelPagination, EmptyCollectionHasOneEmptyPage)
{
    const auto empty = paginate(0, 1, 6);
    ASSERT_TRUE(empty);
    EXPECT_EQ(empty->offset, 0);
    EXPECT_EQ(empty->count, 0);
    EXPECT_EQ(empty->page, 1);
    EXPECT_EQ(empty->page_count, 1);
    EXPECT_FALSE(paginate(0, 2, 6));
}

TEST(PanelPagination, ArithmeticIsBoundedAtMaximumCollectionSize)
{
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    const auto last_single = paginate(maximum, maximum, 1);
    ASSERT_TRUE(last_single);
    EXPECT_EQ(last_single->offset, maximum - 1);
    EXPECT_EQ(last_single->count, 1);
    EXPECT_EQ(last_single->page_count, maximum);
    const auto whole = paginate(maximum, 1, maximum);
    ASSERT_TRUE(whole);
    EXPECT_EQ(whole->offset, 0);
    EXPECT_EQ(whole->count, maximum);
    EXPECT_EQ(whole->page_count, 1);
    EXPECT_FALSE(paginate(maximum, 2, maximum));
    const auto last_page = (maximum - 1) / 6 + 1;
    const auto last = paginate(maximum, last_page, 6);
    ASSERT_TRUE(last);
    EXPECT_EQ(last->count, maximum - last->offset);
    EXPECT_GE(last->count, 1);
    EXPECT_LE(last->count, 6);
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
        EXPECT_NE(text.find("/dg flags"), text.npos);
        for (const auto command : {"/dg pos", "/dg region", "/dg flag ", "/dg trust", "/dg untrust", "/dg reload"}) {
            EXPECT_EQ(text.find(command), text.npos) << command;
        }
        for (const auto section : {Message::HelpSelection, Message::HelpRegions}) {
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

TEST(HelpPanel, CommandRowsPreserveExistingArgumentColorTransitions)
{
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        const auto lines = renderHelp(locale, true);
        for (const auto &entry : commandCatalog()) {
            const auto usage = helpUsage(entry);
            const auto argument_start = usage.find_first_of("<[");
            const auto command = std::string_view(usage).substr(0, argument_start);
            const auto arguments =
                argument_start == usage.npos ? std::string_view{} : std::string_view(usage).substr(argument_start);
            const auto expected =
                std::format("  {}{}{}{}{} / {}{}{}", Theme::White, command, Theme::LightGray, arguments,
                            Theme::DarkGray, Theme::LightGray, messageText(entry.description, locale), Theme::Reset);
            EXPECT_NE(std::ranges::find(lines, expected), lines.end()) << entry.path;
        }
    }
}

TEST(HelpPanel, StaysBoundedAndResetsEveryLine)
{
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        for (const bool can_manage : {false, true}) {
            const auto lines = renderHelp(locale, can_manage);
            EXPECT_LE(lines.size(), 30);
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
    ASSERT_EQ(supportedFlags().size(), 26);
    for (const auto flag : supportedFlags()) {
        EXPECT_TRUE(flags.insert(flagName(flag)).second);
        EXPECT_EQ(parseFlag(flagName(flag)), flag);
    }
    EXPECT_EQ(flags, (std::set<std::string_view>{"build",
                                                 "interact",
                                                 "container-access",
                                                 "pvp",
                                                 "explosions",
                                                 "fluid-flow",
                                                 "block-form",
                                                 "leaf-decay",
                                                 "actor-griefing",
                                                 "mob-spawning",
                                                 "mob-damage",
                                                 "entry",
                                                 "exit",
                                                 "block-break",
                                                 "block-place",
                                                 "use",
                                                 "use-anvil",
                                                 "sleep",
                                                 "item-drop",
                                                 "item-pickup",
                                                 "send-chat",
                                                 "water-flow",
                                                 "lava-flow",
                                                 "fall-damage",
                                                 "firework-damage",
                                                 "invincible"}));
    std::set<std::string_view> states;
    ASSERT_EQ(supportedFlagStates().size(), 3);
    for (const auto state : supportedFlagStates()) {
        EXPECT_TRUE(states.insert(stateName(state)).second);
        EXPECT_EQ(parseState(stateName(state)), state);
    }
    EXPECT_EQ(states, (std::set<std::string_view>{"allow", "deny", "inherit"}));
}

TEST(FlagPresentation, CatalogPagesShowEveryFlagOnceAndLocalizedUsage)
{
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        const auto first = renderFlagCatalogPage(locale, 1, true);
        ASSERT_TRUE(first);
        EXPECT_EQ(first->total_count, 26);
        EXPECT_EQ(first->page_count, 5);
        std::string text;
        for (std::size_t page_number = 1; page_number <= first->page_count; ++page_number) {
            const auto page = renderFlagCatalogPage(locale, page_number, true);
            ASSERT_TRUE(page);
            EXPECT_EQ(page->page, page_number);
            EXPECT_EQ(page->page_count, first->page_count);
            EXPECT_EQ(page->total_count, first->total_count);
            const auto page_text = plainHelp(page->lines);
            text += page_text;
            EXPECT_EQ(page->lines.front(), page->lines.back());
            EXPECT_NE(page_text.find(messageText(Message::FlagsAvailable, locale)), page_text.npos);
            EXPECT_NE(page_text.find(std::format("{}/{}", page_number, first->page_count)), page_text.npos);
            std::size_t count = 0;
            for (const auto flag : supportedFlags()) {
                count += page_text.find(std::format("{} / ", flagName(flag))) != page_text.npos;
            }
            EXPECT_GE(count, 1);
            EXPECT_LE(count, 6);
            for (const auto state : supportedFlagStates()) {
                EXPECT_NE(page_text.find(stateName(state)), page_text.npos);
            }
            EXPECT_NE(page_text.find("/dg flag spawn pvp deny"), page_text.npos);
            EXPECT_NE(page_text.find(locale == Locale::English ? "Usage:" : "Uso:"), page_text.npos);
            EXPECT_NE(page_text.find(messageText(Message::FlagDefaults, locale)), page_text.npos);
            EXPECT_NE(page_text.find(messageText(Message::FlagFallbacks, locale)), page_text.npos);
            for (const auto &line : page->lines) {
                EXPECT_TRUE(line.ends_with(Theme::Reset));
                EXPECT_EQ(line.find('\n'), line.npos);
                EXPECT_LE(plainText(line).size(), 110);
            }
            EXPECT_LE(page->lines.size(), 23);
        }
        for (const auto flag : supportedFlags()) {
            const auto label = std::format("{} / ", flagName(flag));
            const auto found = text.find(label);
            ASSERT_NE(found, text.npos);
            EXPECT_EQ(text.find(label, found + label.size()), text.npos);
        }
        for (const auto description : {Message::FlagBuildDescription,          Message::FlagInteractDescription,
                                       Message::FlagContainerDescription,      Message::FlagPvpDescription,
                                       Message::FlagExplosionsDescription,     Message::FlagFluidFlowDescription,
                                       Message::FlagBlockFormDescription,      Message::FlagLeafDecayDescription,
                                       Message::FlagActorGriefingDescription,  Message::FlagMobSpawningDescription,
                                       Message::FlagMobDamageDescription,      Message::FlagEntryDescription,
                                       Message::FlagExitDescription,           Message::FlagBlockBreakDescription,
                                       Message::FlagBlockPlaceDescription,     Message::FlagUseDescription,
                                       Message::FlagUseAnvilDescription,       Message::FlagSleepDescription,
                                       Message::FlagItemDropDescription,       Message::FlagItemPickupDescription,
                                       Message::FlagSendChatDescription,       Message::FlagWaterFlowDescription,
                                       Message::FlagLavaFlowDescription,       Message::FlagFallDamageDescription,
                                       Message::FlagFireworkDamageDescription, Message::FlagInvincibleDescription}) {
            EXPECT_NE(text.find(messageText(description, locale)), text.npos);
            EXPECT_NE(messageText(description, Locale::English), messageText(description, Locale::Spanish));
        }
    }
}

TEST(FlagPresentation, LegacyCatalogShowsOnlyFirstAdministrativePage)
{
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        const auto first = renderFlagCatalogPage(locale, 1, true);
        ASSERT_TRUE(first);
        EXPECT_EQ(renderFlagCatalog(locale), first->lines);
        const auto text = plainHelp(first->lines);
        EXPECT_NE(text.find("/dg flags 2"), text.npos);
        EXPECT_EQ(text.find("/dg flags 0"), text.npos);
        EXPECT_EQ(text.find("/dg flags 1"), text.npos);
    }
}

TEST(FlagPresentation, InvalidPagesAndScopesAreRejected)
{
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        const auto first = renderFlagCatalogPage(locale);
        ASSERT_TRUE(first);
        EXPECT_FALSE(renderFlagCatalogPage(locale, 0));
        EXPECT_FALSE(renderFlagCatalogPage(locale, first->page_count + 1));
        EXPECT_FALSE(renderFlagCatalogPage(locale, std::numeric_limits<std::size_t>::max()));
        EXPECT_FALSE(renderFlagCatalogPage(locale, 1, false, static_cast<FlagScope>(999)));
    }
}

TEST(FlagPresentation, PublicDiscoveryHidesAdministrativeInstructions)
{
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        const auto first = renderFlagCatalogPage(locale);
        ASSERT_TRUE(first);
        for (std::size_t number = 1; number <= first->page_count; ++number) {
            const auto page = renderFlagCatalogPage(locale, number);
            ASSERT_TRUE(page);
            const auto text = plainHelp(page->lines);
            EXPECT_EQ(text.find("/dg flag "), text.npos);
            EXPECT_EQ(text.find(messageText(Message::FlagExample, locale)), text.npos);
            EXPECT_EQ(text.find(locale == Locale::English ? "Usage:" : "Uso:"), text.npos);
            EXPECT_NE(text.find("/dg flags "), text.npos);
            for (const auto &line : page->lines) {
                EXPECT_TRUE(line.ends_with(Theme::Reset));
                EXPECT_EQ(line.find('\n'), line.npos);
                EXPECT_LE(plainText(line).size(), 110);
            }
            EXPECT_LE(page->lines.size(), 21);
        }
    }
}

TEST(FlagPresentation, NavigationUsesOnlyExistingAdjacentPages)
{
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        const auto first = renderFlagCatalogPage(locale);
        ASSERT_TRUE(first);
        for (std::size_t number = 1; number <= first->page_count; ++number) {
            const auto page = renderFlagCatalogPage(locale, number);
            ASSERT_TRUE(page);
            const auto text = plainHelp(page->lines);
            for (std::size_t candidate = 0; candidate <= first->page_count + 1; ++candidate) {
                const auto present = text.find(std::format("/dg flags {}", candidate)) != text.npos;
                const auto adjacent = candidate >= 1 && candidate <= first->page_count &&
                                      (candidate + 1 == number || candidate == number + 1);
                EXPECT_EQ(present, adjacent) << number << ':' << candidate;
            }
        }
    }
}

TEST(FlagPresentation, ScopeFiltersUseRegistryCategoriesAndKeepPermissions)
{
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        for (const auto scope : {FlagScope::Player, FlagScope::Environment, FlagScope::Transition}) {
            const auto first = renderFlagCatalogPage(locale, 1, false, scope);
            ASSERT_TRUE(first);
            const auto count =
                std::ranges::count_if(supportedFlags(), [scope](Flag flag) { return flagScope(flag) == scope; });
            EXPECT_EQ(first->total_count, static_cast<std::size_t>(count));
            std::string text;
            for (std::size_t number = 1; number <= first->page_count; ++number) {
                const auto page = renderFlagCatalogPage(locale, number, false, scope);
                ASSERT_TRUE(page);
                EXPECT_EQ(page->total_count, first->total_count);
                text += plainHelp(page->lines);
            }
            for (const auto flag : supportedFlags()) {
                EXPECT_EQ(text.find(std::format("{} / ", flagName(flag))) != text.npos, flagScope(flag) == scope);
            }
            for (const auto &[category, heading] : {std::pair{FlagScope::Player, Message::FlagScopePlayer},
                                                    std::pair{FlagScope::Environment, Message::FlagScopeEnvironment},
                                                    std::pair{FlagScope::Transition, Message::FlagScopeTransition}}) {
                EXPECT_EQ(text.find(messageText(heading, locale)) != text.npos, category == scope);
            }
            EXPECT_EQ(text.find("/dg flag "), text.npos);
            EXPECT_EQ(text.find("/dg flags "), text.npos);
            EXPECT_FALSE(renderFlagCatalogPage(locale, first->page_count + 1, false, scope));
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
        EXPECT_NE(text.find(messageText(Message::FlagDefaults, locale)), text.npos);
        for (const auto flag : supportedFlags()) {
            const auto found = before.find(flag);
            const auto state = found == before.end() ? FlagState::Inherit : found->second;
            EXPECT_NE(text.find(std::format("{}: {}", flagName(flag), stateName(state))), text.npos);
        }
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
    for (const auto flag : supportedFlags()) {
        if (flag != Flag::Pvp) {
            EXPECT_EQ(text.find(std::format("{}:", flagName(flag))), text.npos);
        }
    }
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

}
