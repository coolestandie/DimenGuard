#include "dimenguard/command/catalog.h"

#include "dimenguard/region/region.h"

#include <array>
#include <format>
#include <stdexcept>
#include <utility>

namespace dimenguard {
namespace {

using Section = CommandSection;
using Kind = ParameterKind;

template <typename Enum, typename Name>
std::vector<std::string_view> choicesFor(std::span<const Enum> values, Name name)
{
    std::vector<std::string_view> choices;
    choices.reserve(values.size());
    for (const auto value : values) {
        choices.push_back(name(value));
    }
    return choices;
}

const auto &definitions()
{
    // The plugin description builder can run before other translation units initialize.
    static const std::array catalog = {
        CommandSpec{"pos1", Section::Selection, Message::HelpSelectFirst, true, {}},
        CommandSpec{"pos2", Section::Selection, Message::HelpSelectSecond, true, {}},
        CommandSpec{"region create", Section::Regions, Message::HelpCreate, true, {{"name", Kind::Word, false, {}}}},
        CommandSpec{"region delete", Section::Regions, Message::HelpDelete, true, {{"region", Kind::Word, false, {}}}},
        CommandSpec{"region rename",
                    Section::Regions,
                    Message::HelpRename,
                    true,
                    {{"region", Kind::Word, false, {}}, {"name", Kind::Word, false, {}}}},
        CommandSpec{"region list", Section::Regions, Message::HelpList, true, {{"page", Kind::Integer, true, {}}}},
        CommandSpec{"region info", Section::Regions, Message::HelpInfo, true, {{"region", Kind::Word, false, {}}}},
        CommandSpec{"region priority",
                    Section::Regions,
                    Message::HelpPriority,
                    true,
                    {{"region", Kind::Word, false, {}}, {"priority", Kind::Integer, false, {}}}},
        CommandSpec{"flag",
                    Section::Protection,
                    Message::HelpFlag,
                    true,
                    {{"region", Kind::Word, true, {}},
                     {"flag", Kind::Choice, true, choicesFor(supportedFlags(), flagName)},
                     {"value", Kind::Message, true, {}}}},
        CommandSpec{"trust",
                    Section::Protection,
                    Message::HelpTrust,
                    true,
                    {{"region", Kind::Word, false, {}}, {"player", Kind::Player, false, {}}}},
        CommandSpec{"untrust",
                    Section::Protection,
                    Message::HelpUntrust,
                    true,
                    {{"region", Kind::Word, false, {}}, {"player", Kind::Player, false, {}}}},
        CommandSpec{"flags", Section::Protection, Message::HelpFlags, false, {{"page", Kind::Integer, true, {}}}},
        CommandSpec{"help", Section::General, Message::HelpShow, false, {}, true},
        CommandSpec{"reload", Section::General, Message::HelpReload, true, {}},
        CommandSpec{"language",
                    Section::General,
                    Message::HelpLanguage,
                    false,
                    {{"language", Kind::Choice, false, {"en", "es"}}}},
    };
    return catalog;
}

std::string joinChoices(std::span<const std::string_view> choices)
{
    std::string joined;
    for (const auto choice : choices) {
        if (!joined.empty()) {
            joined += '|';
        }
        joined += choice;
    }
    return joined;
}

std::string_view nativeType(ParameterKind kind)
{
    switch (kind) {
    case Kind::Word:
        return "str";
    case Kind::Integer:
        return "int";
    case Kind::Player:
        return "player";
    case Kind::Message:
        return "message";
    case Kind::Choice:
        break;
    }
    throw std::invalid_argument("A choice parameter needs its own named enum");
}

void appendParameter(std::string &usage, std::string_view name, std::string_view type, bool optional,
                     std::span<const std::string_view> choices = {})
{
    usage += ' ';
    if (!choices.empty()) {
        usage += '(' + joinChoices(choices) + ')';
    }
    usage += std::format("{}{}: {}{}", optional ? '[' : '<', name, type, optional ? ']' : '>');
}

std::string nativeUsage(std::string_view root, std::span<const CommandParameter> parameters, bool optional_root,
                        std::size_t index)
{
    std::string usage = "/dg";
    std::size_t argument = 0;
    const auto enum_type = [&] {
        return std::format("DimenGuardCommand{}Arg{}", index, argument++);
    };
    const std::array values{root};
    appendParameter(usage, root, enum_type(), optional_root, values);
    for (const auto &parameter : parameters) {
        const auto type = parameter.kind == Kind::Choice ? enum_type() : std::string(nativeType(parameter.kind));
        appendParameter(usage, parameter.name, type, parameter.optional, parameter.choices);
    }
    return usage;
}

std::vector<CommandParameter> regionParameters()
{
    std::vector<std::string_view> actions;
    constexpr std::string_view prefix = "region ";
    for (const auto &command : definitions()) {
        if (command.path.starts_with(prefix)) {
            actions.push_back(command.path.substr(prefix.size()));
        }
    }
    // Endstone creates a separate enum symbol for every declaration, even for repeated
    // names/values. Bedrock cannot route the shared "region" prefix across those symbols.
    // Register one action enum and validate each action's arity/types in CommandHandler.
    // A message tail accepts both names and numbers; Bedrock's str/Id rejects numeric tokens.
    return {{"action", Kind::Choice, false, std::move(actions)}, {"arguments", Kind::Message, true, {}}};
}

}

std::span<const CommandSpec> commandCatalog()
{
    return definitions();
}

std::vector<CommandSpec> clientCommandCatalog()
{
    std::vector<CommandSpec> catalog;
    for (const auto &command : definitions()) {
        if (command.path != "flag") {
            catalog.push_back(command);
            continue;
        }
        auto query = command;
        query.parameters.resize(1);
        catalog.push_back(std::move(query));
        for (const auto flag : supportedFlags()) {
            auto typed = command;
            typed.parameters[0].optional = false;
            typed.parameters[1].optional = false;
            typed.parameters[1].choices = {flagName(flag)};
            const auto choices = flagValueSuggestions(flag);
            if (!choices.empty() && (flagType(flag) == FlagType::State || flagType(flag) == FlagType::Boolean)) {
                typed.parameters[2].kind = Kind::Choice;
                typed.parameters[2].choices.assign(choices.begin(), choices.end());
            }
            catalog.push_back(std::move(typed));
        }
    }
    return catalog;
}

std::string helpUsage(const CommandSpec &command)
{
    std::string usage = "/dg ";
    usage += command.path;
    for (const auto &parameter : command.parameters) {
        const auto choices = joinChoices(parameter.choices);
        const auto name =
            parameter.kind == Kind::Choice && choices.size() <= 24 ? choices : std::string(parameter.name);
        usage += std::format(" {}{}{}", parameter.optional ? '[' : '<', name, parameter.optional ? ']' : '>');
    }
    return usage;
}

std::vector<std::string> nativeUsages()
{
    const auto catalog = commandCatalog();
    std::vector<std::string> usages;
    usages.reserve(catalog.size());
    bool region_registered = false;
    for (std::size_t index = 0; index < catalog.size(); ++index) {
        const auto &command = catalog[index];
        if (command.path.starts_with("region ")) {
            if (!region_registered) {
                usages.push_back(nativeUsage("region", regionParameters(), false, index));
                region_registered = true;
            }
            continue;
        }
        if (command.path == "flag" || command.path == "trust" || command.path == "untrust") {
            // Numeric region names must reach our parser; native str/Id rejects them.
            const std::array tail{CommandParameter{"arguments", Kind::Message, command.path == "flag", {}}};
            usages.push_back(nativeUsage(command.path, tail, false, index));
        }
        else {
            usages.push_back(nativeUsage(command.path, command.parameters, command.optional_path, index));
        }
    }
    return usages;
}

}
