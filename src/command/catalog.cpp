#include "dimenguard/command/catalog.h"

#include "dimenguard/region/region.h"

#include <array>
#include <format>
#include <stdexcept>

namespace dimenguard {
namespace {

using Section = CommandSection;
using Kind = ParameterKind;

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
        CommandSpec{
            "flag",
            Section::Protection,
            Message::HelpFlag,
            true,
            {{"region", Kind::Word, false, {}},
             {"flag",
              Kind::Choice,
              false,
              {flagName(Flag::Build), flagName(Flag::Interact), flagName(Flag::ContainerAccess), flagName(Flag::Pvp)}},
             {"state",
              Kind::Choice,
              false,
              {stateName(FlagState::Allow), stateName(FlagState::Deny), stateName(FlagState::Inherit)}}}},
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

}  // namespace

std::span<const CommandSpec> commandCatalog()
{
    return definitions();
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
    for (std::size_t index = 0; index < catalog.size(); ++index) {
        const auto &command = catalog[index];
        std::string usage = "/dg";
        std::size_t argument = 0;
        // Endstone registers enums independently per usage. Distinct names avoid collisions
        // when several overloads start with the same literal, such as "region".
        const auto enum_type = [&] {
            return std::format("DimenGuardCommand{}Arg{}", index, argument++);
        };
        auto path = command.path;
        while (!path.empty()) {
            const auto space = path.find(' ');
            const auto literal = path.substr(0, space);
            const std::array values{literal};
            appendParameter(usage, literal, enum_type(), command.optional_path, values);
            path = space == path.npos ? std::string_view{} : path.substr(space + 1);
        }
        for (const auto &parameter : command.parameters) {
            const auto type = parameter.kind == Kind::Choice ? enum_type() : std::string(nativeType(parameter.kind));
            appendParameter(usage, parameter.name, type, parameter.optional, parameter.choices);
        }
        usages.push_back(std::move(usage));
    }
    return usages;
}

}  // namespace dimenguard
