#include "dimenguard/command/arguments.h"
#include "dimenguard/command/catalog.h"
#include "dimenguard/command/flag_input.h"
#include "dimenguard/presentation/flag_panel.h"
#include "dimenguard/rules/spawn_rules.h"
#include "dimenguard/service/region_service.h"
#include "support/database_fixture.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <vector>

namespace dimenguard {
namespace {

const DimensionKey dimension{"world", "minecraft:overworld"};
constexpr BlockPosition inside{-1, 64, -1};

Region region(std::string name = "spawn")
{
    Region result;
    result.key = {dimension, std::move(name)};
    result.bounds = {{-16, 0, -16}, {15, 255, 15}};
    result.owner = "owner";
    return result;
}

TEST(TypedFlagCommands, ParsesPlainAndQuotedMessageTailsWithoutLosingInternalSpaces)
{
    for (const auto &tail : {std::string{"123 entry-deny-message Keep  this space"},
                             std::string{"123 entry-deny-message \"Keep  this space\""}}) {
        const auto args = normalizeCommandArguments(std::vector<std::string>{"flag", tail});
        ASSERT_TRUE(args);
        ASSERT_EQ(args->size(), 4);
        EXPECT_EQ((*args)[1], "123");
        EXPECT_EQ((*args)[3], "Keep  this space");
        const auto change = parseFlagChange(Flag::EntryDenyMessage, args->back());
        ASSERT_TRUE(change && change->value);
        EXPECT_EQ(formatFlagValue(*change->value), "Keep  this space");
    }
    EXPECT_FALSE(normalizeCommandArguments(std::vector<std::string>{"flag", "test entry-deny-message \"broken"}));
    EXPECT_FALSE(normalizeCommandArguments(std::vector<std::string>{"flag", std::string(8193, 'a')}));
}

TEST(TypedFlagCommands, RejectsExtraStateTokensWithoutTreatingThemAsValidState)
{
    const auto args = normalizeCommandArguments(std::vector<std::string>{"flag", "test build deny extra"});
    ASSERT_TRUE(args);
    EXPECT_FALSE(parseFlagChange(Flag::Build, args->back()));
    EXPECT_FALSE(parseFlagChange(Flag::DenySpawn, "zombie"));
    EXPECT_FALSE(parseFlagChange(Flag::DenySpawn, "minecraft:zombie,"));
    EXPECT_FALSE(parseFlagChange(Flag::EntryDenyMessage, "bad\nline"));
}

TEST(TypedFlagCommands, QuotedEmptyTextAndOuterSpacesRemainIntentionalValues)
{
    for (const auto &text : {std::string{}, std::string{"  private  "}}) {
        const auto args =
            normalizeCommandArguments(std::vector<std::string>{"flag", "spawn entry-deny-message \"" + text + "\""});
        ASSERT_TRUE(args);
        ASSERT_EQ(args->size(), 4);
        EXPECT_EQ(args->back(), text);
        const auto change = parseFlagChange(Flag::EntryDenyMessage, args->back());
        ASSERT_TRUE(change && change->value);
        EXPECT_EQ(formatFlagValue(*change->value), text);
    }
}

TEST(TypedFlagCommands, ClearIsDistinctFromEmptySetAndStringInherit)
{
    for (const auto flag : supportedFlags()) {
        const auto clear = parseFlagChange(flag, "--unset");
        ASSERT_TRUE(clear);
        EXPECT_FALSE(clear->value);
    }
    const auto inherit = parseFlagChange(Flag::Build, "inherit");
    ASSERT_TRUE(inherit);
    EXPECT_FALSE(inherit->value);
    const auto text = parseFlagChange(Flag::EntryDenyMessage, "inherit");
    ASSERT_TRUE(text && text->value);
    EXPECT_EQ(formatFlagValue(*text->value), "inherit");
    const auto empty = parseFlagChange(Flag::DenySpawn, "[]");
    ASSERT_TRUE(empty && empty->value);
    ASSERT_NE(empty->value->get<FlagSet>(), nullptr);
    EXPECT_TRUE(empty->value->get<FlagSet>()->empty());
}

TEST(TypedFlagCommands, ClientChoicesComeFromDefinitionsAndDoNotForceStateSyntaxOnTextOrSets)
{
    std::set<Flag> seen;
    std::size_t query_count = 0;
    for (const auto &command : clientCommandCatalog()) {
        if (command.path != "flag") {
            continue;
        }
        if (command.parameters.size() == 1) {
            ++query_count;
            EXPECT_TRUE(command.parameters.front().optional);
            continue;
        }
        ASSERT_EQ(command.parameters.size(), 3);
        EXPECT_FALSE(command.parameters[0].optional);
        EXPECT_FALSE(command.parameters[1].optional);
        ASSERT_EQ(command.parameters[1].choices.size(), 1);
        const auto flag = parseFlag(command.parameters[1].choices.front());
        ASSERT_TRUE(flag);
        EXPECT_TRUE(seen.insert(*flag).second);
        const auto &parameter = command.parameters[2];
        EXPECT_TRUE(parameter.optional);
        if (flagType(*flag) == FlagType::State) {
            EXPECT_EQ(parameter.kind, ParameterKind::Choice);
            const auto suggestions = flagValueSuggestions(*flag);
            EXPECT_EQ(parameter.choices, (std::vector<std::string_view>{suggestions.begin(), suggestions.end()}));
            EXPECT_EQ(std::set<std::string_view>(parameter.choices.begin(), parameter.choices.end()).size(),
                      parameter.choices.size());
        }
        else {
            EXPECT_EQ(parameter.kind, ParameterKind::Message);
            EXPECT_TRUE(parameter.choices.empty());
        }
    }
    EXPECT_EQ(query_count, 1);
    EXPECT_EQ(seen.size(), supportedFlags().size());
}

TEST(TypedFlagPresentation, RendersRegisteredTypesAndValuesInBothLocales)
{
    auto value = region();
    value.flags[Flag::EntryDenyMessage] = FlagValue("Private grounds");
    value.flags[Flag::DenySpawn] = FlagValue(FlagSet{"minecraft:zombie"});
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        for (const auto flag : {Flag::EntryDenyMessage, Flag::DenySpawn, Flag::Tnt}) {
            const auto lines = renderRegionFlags(value, locale, flag);
            std::string text;
            for (const auto &line : lines) {
                text += line;
            }
            EXPECT_NE(text.find(valueTypeName(flagType(flag))), text.npos);
            EXPECT_NE(text.find("--unset"), text.npos);
            if (value.flags.contains(flag)) {
                EXPECT_NE(text.find(formatFlagValue(value.flags.at(flag))), text.npos);
            }
        }
    }
}

TEST(TypedFlagPresentation, DistinguishesUnsetLiteralInheritAndEmptyText)
{
    auto value = region();
    EXPECT_EQ(displayFlagValue(FlagValue("inherit")), "\"inherit\"");
    EXPECT_EQ(displayFlagValue(FlagValue("")), "\"\"");
    EXPECT_EQ(displayFlagValue(FlagValue("say \"hello\"")), "\"say \\\"hello\\\"\"");
    for (const auto locale : {Locale::English, Locale::Spanish}) {
        const auto unset = renderRegionFlags(value, locale, Flag::EntryDenyMessage);
        EXPECT_TRUE(std::ranges::any_of(unset, [&](const std::string &line) {
            return line.find(messageText(Message::FlagValueUnset, locale)) != line.npos;
        }));
    }
}

TEST(SpawnRules, DenySetAppliesToActorsButNeverPlayersAndKeepsMobSpawningIndependent)
{
    auto value = region();
    value.flags[Flag::DenySpawn] = FlagValue(FlagSet{"minecraft:zombie", "minecraft:arrow", "minecraft:player"});
    RegionManager manager;
    manager.replaceAll({value});
    EXPECT_FALSE(SpawnRules::isAllowed(manager, dimension, inside, "minecraft:zombie", SpawnSubject::Mob));
    EXPECT_FALSE(SpawnRules::isAllowed(manager, dimension, inside, "minecraft:arrow", SpawnSubject::Actor));
    EXPECT_TRUE(SpawnRules::isAllowed(manager, dimension, inside, "minecraft:player", SpawnSubject::Player));
    EXPECT_TRUE(SpawnRules::isAllowed(manager, dimension, inside, "custom:zombie", SpawnSubject::Mob));
    EXPECT_TRUE(SpawnRules::isAllowed(manager, dimension, {16, 64, -1}, "minecraft:zombie", SpawnSubject::Mob));
    EXPECT_TRUE(
        SpawnRules::isAllowed(manager, {"world", "minecraft:nether"}, inside, "minecraft:zombie", SpawnSubject::Mob));
    value.flags[Flag::MobSpawning] = FlagState::Deny;
    value.flags[Flag::DenySpawn] = FlagValue(FlagSet{});
    manager.replaceAll({value});
    EXPECT_FALSE(SpawnRules::isAllowed(manager, dimension, inside, "minecraft:cow", SpawnSubject::Mob));
    EXPECT_TRUE(SpawnRules::isAllowed(manager, dimension, inside, "minecraft:arrow", SpawnSubject::Actor));
}

using TypedFlagService = test::DatabaseFixture;

TEST_F(TypedFlagService, RejectsInvalidTypedValueAndPreservesStoredAndLiveProtection)
{
    RegionService service(path_);
    const auto value = region();
    service.create(value);
    service.setFlagValue(value.key, Flag::DenySpawn, FlagValue(FlagSet{"minecraft:zombie"}));
    EXPECT_THROW(service.setFlagValue(value.key, Flag::DenySpawn, FlagValue("minecraft:cow")), std::invalid_argument);
    EXPECT_THROW(service.setFlagValue(value.key, Flag::EntryDenyMessage, FlagValue("bad\nline")),
                 std::invalid_argument);
    const auto stored = service.getRegions().find(value.key)->flags;
    executeRaw("CREATE TRIGGER reject_flag BEFORE INSERT ON flags BEGIN SELECT RAISE(ABORT, 'injected'); END");
    EXPECT_THROW(service.setFlagValue(value.key, Flag::DenySpawn, FlagValue(FlagSet{"minecraft:cow"})),
                 std::runtime_error);
    EXPECT_EQ(service.getRegions().find(value.key)->flags, stored);
    EXPECT_FALSE(SpawnRules::isAllowed(service.getRegions(), dimension, inside, "minecraft:zombie", SpawnSubject::Mob));
    RegionService reopened(path_);
    ASSERT_NE(reopened.getRegions().find(value.key), nullptr);
    EXPECT_EQ(reopened.getRegions().find(value.key)->flags, stored);
}

TEST_F(TypedFlagService, ClearingValuePreservesItsIndependentGroup)
{
    RegionService service(path_);
    const auto value = region();
    service.create(value);
    service.setFlagGroup(value.key, Flag::EntryDenyMessage, RegionGroup::NonMembers);
    service.setFlagValue(value.key, Flag::EntryDenyMessage, FlagValue("inherit"));
    service.setFlagValue(value.key, Flag::EntryDenyMessage, std::nullopt);
    const auto *actual = service.getRegions().find(value.key);
    ASSERT_NE(actual, nullptr);
    EXPECT_FALSE(actual->flags.contains(Flag::EntryDenyMessage));
    EXPECT_EQ(actual->flag_groups.at(Flag::EntryDenyMessage), RegionGroup::NonMembers);
    service.reload();
    EXPECT_EQ(service.getRegions().find(value.key)->flag_groups.at(Flag::EntryDenyMessage), RegionGroup::NonMembers);
}

}
}
