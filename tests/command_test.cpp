#include "dimenguard/command/catalog.h"
#include "dimenguard/command/parse.h"
#include "dimenguard/region/region.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace dimenguard {
namespace {

const CommandSpec &findCommand(std::string_view path)
{
    const auto catalog = commandCatalog();
    const auto found = std::ranges::find(catalog, path, &CommandSpec::path);
    if (found == catalog.end()) {
        throw std::runtime_error("Expected command path is absent: " + std::string(path));
    }
    return *found;
}

std::string usageFor(std::string_view path)
{
    const auto catalog = commandCatalog();
    const auto usages = nativeUsages();
    for (std::size_t index = 0; index < catalog.size(); ++index) {
        if (catalog[index].path == path) {
            return usages.at(index);
        }
    }
    throw std::runtime_error("Expected command path is absent: " + std::string(path));
}

TEST(CommandCatalog, ContainsEveryCommandPathExactlyOnce)
{
    const std::set<std::string_view> expected{
        "pos1",        "pos2",        "region create",   "region delete", "region rename",
        "region list", "region info", "region priority", "flag",          "trust",
        "untrust",     "help",        "reload",          "language",
    };
    const auto catalog = commandCatalog();
    ASSERT_EQ(catalog.size(), 14);
    std::set<std::string_view> actual;
    for (const auto &command : catalog) {
        EXPECT_TRUE(actual.insert(command.path).second) << command.path;
        EXPECT_FALSE(helpUsage(command).empty());
    }
    EXPECT_EQ(actual, expected);
}

TEST(CommandCatalog, OnlyHelpAndLanguageArePublic)
{
    for (const auto &command : commandCatalog()) {
        const bool public_command = command.path == "help" || command.path == "language";
        EXPECT_EQ(command.requires_admin, !public_command) << command.path;
        EXPECT_EQ(command.optional_path, command.path == "help") << command.path;
    }
    EXPECT_NE(usageFor("help").find("(help)[help: "), std::string::npos);
}

TEST(CommandCatalog, NativeGrammarUsesRequiredPlayersAndTypedIntegers)
{
    for (const auto path : {"trust", "untrust"}) {
        SCOPED_TRACE(path);
        const auto &command = findCommand(path);
        ASSERT_EQ(command.parameters.size(), 2);
        EXPECT_EQ(command.parameters[0].name, "region");
        EXPECT_EQ(command.parameters[0].kind, ParameterKind::Word);
        EXPECT_FALSE(command.parameters[0].optional);
        EXPECT_EQ(command.parameters[1].name, "player");
        EXPECT_EQ(command.parameters[1].kind, ParameterKind::Player);
        EXPECT_FALSE(command.parameters[1].optional);
        EXPECT_TRUE(usageFor(path).ends_with(" <region: str> <player: player>"));
        EXPECT_EQ(helpUsage(command), "/dg " + std::string(path) + " <region> <player>");
    }
    EXPECT_TRUE(usageFor("region priority").ends_with(" <region: str> <priority: int>"));
    EXPECT_TRUE(usageFor("region list").ends_with(" [page: int]"));
    EXPECT_TRUE(usageFor("region create").ends_with(" <name: str>"));
    EXPECT_TRUE(usageFor("region rename").ends_with(" <region: str> <name: str>"));
}

TEST(CommandCatalog, FlagStateAndLanguageChoicesMatchSupportedValues)
{
    const auto &flag_command = findCommand("flag");
    ASSERT_EQ(flag_command.parameters.size(), 3);
    const auto &flags = flag_command.parameters[1];
    const auto &states = flag_command.parameters[2];
    EXPECT_EQ(flags.kind, ParameterKind::Choice);
    EXPECT_EQ(states.kind, ParameterKind::Choice);
    EXPECT_FALSE(flags.optional);
    EXPECT_FALSE(states.optional);
    EXPECT_EQ(flags.choices, (std::vector<std::string_view>{"build", "interact", "container-access", "pvp"}));
    EXPECT_EQ(states.choices, (std::vector<std::string_view>{"allow", "deny", "inherit"}));
    for (const auto choice : flags.choices) {
        EXPECT_TRUE(parseFlag(choice)) << choice;
    }
    for (const auto choice : states.choices) {
        EXPECT_TRUE(parseState(choice)) << choice;
    }
    EXPECT_NE(usageFor("flag").find("(build|interact|container-access|pvp)<flag: "), std::string::npos);
    EXPECT_NE(usageFor("flag").find("(allow|deny|inherit)<state: "), std::string::npos);
    EXPECT_EQ(helpUsage(flag_command), "/dg flag <region> <flag> <allow|deny|inherit>");

    const auto &language = findCommand("language");
    ASSERT_EQ(language.parameters.size(), 1);
    EXPECT_EQ(language.parameters[0].kind, ParameterKind::Choice);
    EXPECT_FALSE(language.parameters[0].optional);
    EXPECT_EQ(language.parameters[0].choices, (std::vector<std::string_view>{"en", "es"}));
    EXPECT_NE(usageFor("language").find("(en|es)<language: "), std::string::npos);
}

TEST(CommandCatalog, EveryNativeOverloadAndEnumNameIsUnique)
{
    const auto usages = nativeUsages();
    ASSERT_EQ(usages.size(), commandCatalog().size());
    std::set<std::string> seen_usages;
    std::set<std::string> seen_enums;
    const std::regex enum_declaration{R"(\([^)]*\)[<\[][a-z0-9_-]+: ([A-Za-z][A-Za-z0-9_]*)[>\]])"};
    for (const auto &usage : usages) {
        SCOPED_TRACE(usage);
        EXPECT_TRUE(usage.starts_with("/dg "));
        EXPECT_TRUE(seen_usages.insert(usage).second);
        EXPECT_EQ(usage.find("argument1"), std::string::npos);
        EXPECT_EQ(usage.find("UUID"), std::string::npos);
        EXPECT_EQ(usage.find("<action: str>"), std::string::npos);
        const auto begin = std::sregex_iterator(usage.begin(), usage.end(), enum_declaration);
        ASSERT_NE(begin, std::sregex_iterator{});
        for (auto current = begin; current != std::sregex_iterator{}; ++current) {
            const auto name = (*current)[1].str();
            EXPECT_TRUE(seen_enums.insert(name).second) << name;
        }
    }
    EXPECT_GT(seen_enums.size(), usages.size());
}

TEST(PlayerNameParsing, AcceptsExactNamesWithOptionalBalancedQuotes)
{
    EXPECT_EQ(parsePlayerName("IKyel0I"), "IKyel0I");
    EXPECT_EQ(parsePlayerName("\"IKyel0I\""), "IKyel0I");
    EXPECT_EQ(parsePlayerName("Player With Spaces"), "Player With Spaces");
    EXPECT_EQ(parsePlayerName("\"Player With Spaces\""), "Player With Spaces");
}

TEST(PlayerNameParsing, RejectsSelectorsEvenInsideQuotes)
{
    for (const auto name : {"@a", "@e", "@s", "@p", "@r", "@e[type=player,c=1]", "@a[name=IKyel0I]", "\"@a\"",
                            "\"@e[type=player,c=1]\""}) {
        EXPECT_FALSE(parsePlayerName(name)) << name;
    }
}

TEST(PlayerNameParsing, RejectsEmptyUnmatchedQuotedAndControlInputs)
{
    for (const auto name : {"", "\"\"", " ", " Player", "Player ", "\" @a\"", "\"Player", "Player\"", "Player\"Name",
                            "\"\"Player\"\"", "Player\nName", "Player\rName", "Player\tName"}) {
        EXPECT_FALSE(parsePlayerName(name)) << name;
    }
    EXPECT_FALSE(parsePlayerName(std::string_view{"Player\0Name", 11}));
    EXPECT_FALSE(parsePlayerName(std::string_view{"Player\x7fName"}));
}

TEST(PlayerNameMatching, CaseInsensitiveMatchStillRequiresTheEntireName)
{
    EXPECT_TRUE(playerNamesMatch("IKyel0I", "ikyel0i"));
    EXPECT_TRUE(playerNamesMatch("Player With Spaces", "pLaYeR wItH sPaCeS"));
    EXPECT_FALSE(playerNamesMatch("Player", "Player With Spaces"));
    EXPECT_FALSE(playerNamesMatch("Player With Spaces", "Player"));
    EXPECT_FALSE(playerNamesMatch("IKyel0I", "IKyel0I2"));
    EXPECT_FALSE(playerNamesMatch("IKyel0I", "IKyeI0I"));
    EXPECT_FALSE(playerNamesMatch("Player One", "Player  One"));
}

}  // namespace
}  // namespace dimenguard
