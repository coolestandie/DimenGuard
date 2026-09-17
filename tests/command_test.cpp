#include "dimenguard/command/arguments.h"
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
    const auto usages = nativeUsages();
    const auto root = path.substr(0, path.find(' '));
    const auto prefix = "/dg (" + std::string(root) + ")";
    for (const auto &usage : usages) {
        if (usage.starts_with(prefix)) {
            return usage;
        }
    }
    throw std::runtime_error("Expected command path is absent: " + std::string(path));
}

TEST(CommandCatalog, ContainsEveryCommandPathExactlyOnce)
{
    const std::set<std::string_view> expected{
        "pos1",
        "pos2",
        "inspect",
        "region create",
        "region claim",
        "region delete",
        "region rename",
        "region redefine",
        "region move",
        "region list",
        "region info",
        "region flags",
        "region priority",
        "region set-parent",
        "region set-passthrough",
        "region set-flag",
        "region unset-flag",
        "region select",
        "flag",
        "trust",
        "untrust",
        "help",
        "reload",
        "language",
        "flags",
    };
    const auto catalog = commandCatalog();
    ASSERT_EQ(catalog.size(), 25);
    std::set<std::string_view> actual;
    for (const auto &command : catalog) {
        EXPECT_TRUE(actual.insert(command.path).second) << command.path;
        EXPECT_FALSE(helpUsage(command).empty());
    }
    EXPECT_EQ(actual, expected);
}

TEST(CommandCatalog, OnlyHelpLanguageAndFlagDiscoveryArePublic)
{
    for (const auto &command : commandCatalog()) {
        const bool public_command = command.path == "help" || command.path == "language" || command.path == "flags";
        EXPECT_EQ(command.requires_admin, !public_command) << command.path;
        EXPECT_EQ(command.optional_path, command.path == "help") << command.path;
    }
    EXPECT_NE(usageFor("help").find("(help)[help: "), std::string::npos);
}

TEST(CommandCatalog, LogicalGrammarUsesPlayersAndNativeTailsAcceptNumericNames)
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
        EXPECT_TRUE(usageFor(path).ends_with(" <arguments: message>"));
        EXPECT_EQ(helpUsage(command), "/dg " + std::string(path) + " <region> <player>");
    }
    EXPECT_EQ(findCommand("region priority").parameters[1].kind, ParameterKind::Integer);
    EXPECT_EQ(findCommand("region list").parameters[0].kind, ParameterKind::Integer);
    EXPECT_EQ(helpUsage(findCommand("region priority")), "/dg region priority <region> <priority>");
    EXPECT_EQ(helpUsage(findCommand("region list")), "/dg region list [page]");
    EXPECT_EQ(helpUsage(findCommand("region create")), "/dg region create <name>");
    EXPECT_EQ(helpUsage(findCommand("region rename")), "/dg region rename <region> <name>");
}

TEST(CommandCatalog, FlagNamesAndLanguageChoicesMatchRegistryAndValueIsTypedByClientCatalog)
{
    const auto &flag_command = findCommand("flag");
    ASSERT_EQ(flag_command.parameters.size(), 3);
    const auto &flags = flag_command.parameters[1];
    const auto &states = flag_command.parameters[2];
    EXPECT_EQ(flags.kind, ParameterKind::Choice);
    EXPECT_EQ(states.kind, ParameterKind::Message);
    EXPECT_TRUE(flag_command.parameters[0].optional);
    EXPECT_TRUE(flags.optional);
    EXPECT_TRUE(states.optional);
    std::vector<std::string_view> supported_names;
    for (const auto flag : supportedFlags()) {
        supported_names.push_back(flagName(flag));
    }
    EXPECT_EQ(flags.choices, supported_names);
    EXPECT_TRUE(states.choices.empty());
    for (const auto choice : flags.choices) {
        EXPECT_TRUE(parseFlag(choice)) << choice;
    }
    for (const auto choice : states.choices) {
        EXPECT_TRUE(parseState(choice)) << choice;
    }
    EXPECT_TRUE(usageFor("flag").ends_with(" [arguments: message]"));
    EXPECT_EQ(helpUsage(flag_command), "/dg flag [region] [flag] [value]");

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
    ASSERT_EQ(usages.size(), 11);
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

TEST(CommandCatalog, NativeRootsNeverOverlapAcrossOverloads)
{
    // Unique enum TYPE names do not prevent ambiguous enum VALUES in Bedrock.
    // The old six declarations of (region) passed metadata tests but broke actual parsing.
    const std::regex first_enum{R"(^/dg \(([^)]+)\))"};
    std::set<std::string> roots;
    for (const auto &usage : nativeUsages()) {
        std::smatch match;
        ASSERT_TRUE(std::regex_search(usage, match, first_enum)) << usage;
        const auto values = match[1].str();
        std::string_view remaining = values;
        while (!remaining.empty()) {
            const auto separator = remaining.find('|');
            EXPECT_TRUE(roots.insert(std::string(remaining.substr(0, separator))).second) << usage;
            remaining = separator == remaining.npos ? std::string_view{} : remaining.substr(separator + 1);
        }
    }
    EXPECT_EQ(roots, (std::set<std::string>{"pos1", "pos2", "inspect", "region", "flag", "trust", "untrust", "help",
                                            "reload", "language", "flags"}));
}

TEST(CommandCatalog, RegionActionsShareOneOverloadAndComeFromDetailedCatalog)
{
    const auto usage = usageFor("region list");
    const std::regex action_enum{R"(\(([^)]+)\)<action: [A-Za-z0-9_]+>)"};
    std::smatch match;
    ASSERT_TRUE(std::regex_search(usage, match, action_enum));
    std::string expected_actions;
    for (const auto &command : commandCatalog()) {
        if (command.path.starts_with("region ")) {
            if (!expected_actions.empty()) {
                expected_actions += '|';
            }
            expected_actions += command.path.substr(7);
            for (const auto alias : commandAliases(command.path)) {
                expected_actions += '|';
                expected_actions += alias;
            }
            EXPECT_EQ(usageFor(command.path), usage);
        }
    }
    EXPECT_EQ(match[1].str(), expected_actions);
    EXPECT_TRUE(usage.ends_with(" [arguments: message]"));
    EXPECT_EQ(usage.find("[name_or_page: str]"), std::string::npos);
}

TEST(CommandCatalog, RegionAliasesShareCanonicalSemantics)
{
    EXPECT_TRUE(std::ranges::equal(commandAliases("region create"), std::vector<std::string_view>{"define"}));
    EXPECT_TRUE(std::ranges::equal(commandAliases("region delete"), std::vector<std::string_view>{"remove"}));
    EXPECT_TRUE(std::ranges::equal(commandAliases("region priority"), std::vector<std::string_view>{"set-priority"}));
    EXPECT_TRUE(commandAliases("region info").empty());

    const auto client_catalog = clientCommandCatalog();
    const auto has_path = [&client_catalog](std::string_view path) {
        return std::ranges::any_of(client_catalog, [path](const CommandSpec &command) { return command.path == path; });
    };
    EXPECT_TRUE(has_path("region create"));
    EXPECT_TRUE(has_path("region define"));
    EXPECT_TRUE(has_path("region delete"));
    EXPECT_TRUE(has_path("region remove"));
    EXPECT_TRUE(has_path("region priority"));
    EXPECT_TRUE(has_path("region set-priority"));
}

TEST(CommandNormalization, SplitsOnlyNativeMessageTails)
{
    const auto normalize = [](std::initializer_list<std::string> args) {
        return normalizeCommandArguments(std::vector<std::string>(args));
    };
    EXPECT_EQ(normalize({}), std::vector<std::string>{});
    EXPECT_EQ(normalize({"language", "es"}), (std::vector<std::string>{"language", "es"}));
    EXPECT_EQ(normalize({"region", "list"}), (std::vector<std::string>{"region", "list"}));
    EXPECT_EQ(normalize({"region", "priority", "123 -1"}),
              (std::vector<std::string>{"region", "priority", "123", "-1"}));
    EXPECT_EQ(normalize({"flag"}), (std::vector<std::string>{"flag"}));
    EXPECT_EQ(normalize({"flag", "123 build deny"}), (std::vector<std::string>{"flag", "123", "build", "deny"}));
    EXPECT_EQ(normalize({"trust", "123 \"Player With Spaces\""}),
              (std::vector<std::string>{"trust", "123", "Player With Spaces"}));
    EXPECT_EQ(normalize({"untrust", "123 IKyel0I"}), (std::vector<std::string>{"untrust", "123", "IKyel0I"}));
    EXPECT_FALSE(normalize({"region"}));
    EXPECT_EQ(normalize({"region", "rename", "a b extra"}),
              (std::vector<std::string>{"region", "rename", "a", "b", "extra"}));
    EXPECT_EQ(normalize({"flag", "test build deny extra"}),
              (std::vector<std::string>{"flag", "test", "build", "deny extra"}));
    EXPECT_FALSE(normalize({"trust", "test a b"}));
    EXPECT_FALSE(normalize({"trust", "test \"broken"}));
    EXPECT_FALSE(normalize({"region", "list", "1", "2"}));
    EXPECT_EQ(normalize({"region", "list", "survival minecraft:overworld 2"}),
              (std::vector<std::string>{"region", "list", "survival", "minecraft:overworld", "2"}));
    EXPECT_EQ(
        normalize({"region", "set-flag", "survival minecraft:overworld spawn pvp deny"}),
        (std::vector<std::string>{"region", "set-flag", "survival", "minecraft:overworld", "spawn", "pvp", "deny"}));
}

TEST(CommandArgumentsParsing, PreservesListPriorityAndRenameArguments)
{
    EXPECT_EQ(parseCommandArguments("1", 2), (std::vector<std::string>{"1"}));
    EXPECT_EQ(parseCommandArguments("test -1", 2), (std::vector<std::string>{"test", "-1"}));
    EXPECT_EQ(parseCommandArguments("test renamed", 2), (std::vector<std::string>{"test", "renamed"}));
    EXPECT_EQ(parseCommandArguments("123 2", 2), (std::vector<std::string>{"123", "2"}));
    EXPECT_EQ(parseCommandArguments("  test   -2147483648  ", 2), (std::vector<std::string>{"test", "-2147483648"}));
    EXPECT_EQ(parseCommandArguments("test 2147483647", 2), (std::vector<std::string>{"test", "2147483647"}));
}

TEST(CommandArgumentsParsing, LeavesNumberValidationToTheActionHandler)
{
    for (const auto number : {"2147483648", "-2147483649", "1x", "1.5"}) {
        SCOPED_TRACE(number);
        const auto arguments = parseCommandArguments("test " + std::string(number), 2);
        ASSERT_TRUE(arguments);
        ASSERT_EQ(arguments->size(), 2);
        EXPECT_EQ(arguments->front(), "test");
        EXPECT_EQ(arguments->back(), number);
        EXPECT_FALSE(parseInteger(arguments->back()));
    }
}

TEST(CommandArgumentsParsing, PreservesQuotedContentAsOneArgumentWithoutEscapes)
{
    EXPECT_EQ(parseCommandArguments("\"test\" -1", 2), (std::vector<std::string>{"test", "-1"}));
    EXPECT_EQ(parseCommandArguments("\"old name\" \"new name\"", 2),
              (std::vector<std::string>{"old name", "new name"}));
    EXPECT_EQ(parseCommandArguments("  \" spaced  name \"  ", 1), (std::vector<std::string>{" spaced  name "}));
    EXPECT_EQ(parseCommandArguments("\"\"", 1), (std::vector<std::string>{""}));
    EXPECT_EQ(parseCommandArguments("\"\" \"\"", 2), (std::vector<std::string>{"", ""}));
    EXPECT_EQ(parseCommandArguments(R"(test\nname)", 1), (std::vector<std::string>{R"(test\nname)"}));
}

TEST(CommandArgumentsParsing, EmptyOrOnlySpaceInputContainsNoArguments)
{
    for (const auto text : {"", " ", "    "}) {
        SCOPED_TRACE(text);
        EXPECT_EQ(parseCommandArguments(text, 2), std::vector<std::string>{});
        EXPECT_EQ(parseCommandArguments(text, 0), std::vector<std::string>{});
    }
}

TEST(CommandArgumentsParsing, RejectsUnbalancedOrEmbeddedQuotes)
{
    for (const auto text : {"\"test", "test\"", "te\"st", "\"test\"suffix", "prefix\"test\"", "\"first\"\"second\"",
                            "first \"second", R"("first\"second")"}) {
        EXPECT_FALSE(parseCommandArguments(text, 2)) << text;
    }
}

TEST(CommandArgumentsParsing, RejectsEveryAsciiControlEvenInsideQuotes)
{
    for (int code = 0; code <= 127; ++code) {
        if (code >= 32 && code != 127) {
            continue;
        }
        SCOPED_TRACE(code);
        const auto control = std::string(1, static_cast<char>(code));
        EXPECT_FALSE(parseCommandArguments(control + "test", 2));
        EXPECT_FALSE(parseCommandArguments("test" + control, 2));
        EXPECT_FALSE(parseCommandArguments("first" + control + "second", 2));
        EXPECT_FALSE(parseCommandArguments("\"first" + control + "second\"", 2));
    }
}

TEST(CommandArgumentsParsing, EnforcesTheMaximumNumberOfWholeArguments)
{
    EXPECT_FALSE(parseCommandArguments("test", 0));
    EXPECT_FALSE(parseCommandArguments("\"\"", 0));
    EXPECT_FALSE(parseCommandArguments("test renamed", 1));
    EXPECT_FALSE(parseCommandArguments("test renamed extra", 2));
    EXPECT_FALSE(parseCommandArguments("\"first name\" \"second name\" third", 2));
    EXPECT_EQ(parseCommandArguments("\"first name\"", 1), (std::vector<std::string>{"first name"}));
    EXPECT_EQ(parseCommandArguments("test renamed", 2), (std::vector<std::string>{"test", "renamed"}));
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

}
}
