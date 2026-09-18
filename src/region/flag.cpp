#include "dimenguard/region/flag.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace dimenguard {
namespace {
constexpr FlagConstraint constraint(Flag flag)
{
    if (flag == Flag::EntryDenyMessage || flag == Flag::ExitDenyMessage) {
        return FlagConstraint::PlainText;
    }
    if (flag == Flag::DenySpawn) {
        return FlagConstraint::ActorIds;
    }
    return flag == Flag::NonPlayerProtectionDomains ? FlagConstraint::DomainIds : FlagConstraint::None;
}
constexpr std::array definitions{
#define DG_FLAG(id, name, scope, fallback, type, aggregate, description)                  \
    FlagDefinition{Flag::id,       name,      FlagScope::scope,    FlagDefault::fallback, \
                   FlagType::type, aggregate, constraint(Flag::id)},
#include "dimenguard/region/flags.inc"
#undef DG_FLAG
};
constexpr std::array state_names{
    std::pair{FlagState::Inherit, std::string_view{"inherit"}},
    std::pair{FlagState::Allow, std::string_view{"allow"}},
    std::pair{FlagState::Deny, std::string_view{"deny"}},
};
constexpr std::array group_names{
    std::pair{RegionGroup::All, std::string_view{"all"}},
    std::pair{RegionGroup::Members, std::string_view{"members"}},
    std::pair{RegionGroup::Owners, std::string_view{"owners"}},
    std::pair{RegionGroup::NonMembers, std::string_view{"nonmembers"}},
    std::pair{RegionGroup::NonOwners, std::string_view{"nonowners"}},
};
constexpr auto groups = [] {
    std::array<RegionGroup, group_names.size()> values{};
    for (std::size_t index = 0; index < values.size(); ++index) {
        values[index] = group_names[index].first;
    }
    return values;
}();
constexpr auto flags = [] {
    std::array<Flag, definitions.size()> values{};
    for (std::size_t index = 0; index < values.size(); ++index) {
        values[index] = definitions[index].flag;
    }
    return values;
}();
static_assert([] {
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        const auto &entry = definitions[index];
        if (static_cast<std::size_t>(entry.flag) != index ||
            (entry.aggregate && static_cast<std::size_t>(*entry.aggregate) >= index)) {
            return false;
        }
    }
    return true;
}());
constexpr auto states = [] {
    std::array<FlagState, state_names.size()> values{};
    for (std::size_t index = 0; index < values.size(); ++index) {
        values[index] = state_names[index].first;
    }
    return values;
}();
}
const FlagDefinition &flagDefinition(Flag flag)
{
    const auto index = static_cast<std::size_t>(flag);
    if (index >= definitions.size()) {
        throw std::invalid_argument("Unknown region flag");
    }
    return definitions[index];
}
std::string_view flagName(Flag flag)
{
    return flagDefinition(flag).name;
}
FlagScope flagScope(Flag flag)
{
    return flagDefinition(flag).scope;
}
FlagDefault flagDefault(Flag flag)
{
    return flagDefinition(flag).fallback;
}
std::optional<Flag> flagFallback(Flag flag)
{
    return flagDefinition(flag).aggregate;
}
std::span<const FlagDefinition> flagDefinitions()
{
    return definitions;
}
FlagType flagType(Flag flag)
{
    return flagDefinition(flag).type;
}
std::optional<FlagValue> flagDefaultValue(Flag flag)
{
    switch (flagDefault(flag)) {
    case FlagDefault::Allow:
        return FlagState::Allow;
    case FlagDefault::Deny:
        return FlagState::Deny;
    case FlagDefault::EmptySet:
        return FlagValue(FlagSet{});
    case FlagDefault::Members:
    case FlagDefault::None:
        return std::nullopt;
    }
    throw std::invalid_argument("Unknown flag default");
}
void validateFlagValue(Flag flag, const FlagValue &value)
{
    const auto &definition = flagDefinition(flag);
    validateFlagValue(definition.type, value);
    if (definition.constraint == FlagConstraint::None) {
        return;
    }
    if (definition.constraint == FlagConstraint::PlainText) {
        if (value.get<std::string>()->find("\xc2\xa7") != std::string::npos) {
            throw std::invalid_argument("Denial messages must not contain chat formatting codes");
        }
        return;
    }
    for (const auto &entry : *value.get<FlagSet>()) {
        if (definition.constraint == FlagConstraint::ActorIds) {
            const auto separator = entry.find(':');
            if (separator == std::string::npos || separator == 0 || separator + 1 == entry.size() ||
                entry.find(':', separator + 1) != std::string::npos ||
                !std::ranges::all_of(entry,
                                     [](char character) {
                                         return (character >= 'a' && character <= 'z') ||
                                                (character >= '0' && character <= '9') || character == ':' ||
                                                character == '_' || character == '-' || character == '.' ||
                                                character == '/';
                                     }) ||
                entry.substr(0, separator).find('/') != std::string::npos) {
                throw std::invalid_argument("Actor identifiers require namespace:name");
            }
        }
        else if (entry.size() > 64 || !std::ranges::all_of(entry, [](char character) {
                     return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
                            character == '-' || character == '_';
                 })) {
            throw std::invalid_argument("Protection domains require 1-64 canonical identifier characters");
        }
    }
}
std::optional<FlagValue> parseFlagValue(Flag flag, std::string_view text)
{
    auto value = parseFlagValue(flagType(flag), text);
    if (!value) {
        return std::nullopt;
    }
    try {
        validateFlagValue(flag, *value);
        return value;
    }
    catch (const std::invalid_argument &) {
        return std::nullopt;
    }
}
std::span<const std::string_view> flagValueSuggestions(Flag flag)
{
    static constexpr std::array states{std::string_view{"allow"}, std::string_view{"deny"}, std::string_view{"inherit"},
                                       std::string_view{"--unset"}};
    static constexpr std::array sets{std::string_view{"[]"}, std::string_view{"--unset"}};
    static constexpr std::array booleans{std::string_view{"true"}, std::string_view{"false"},
                                         std::string_view{"--unset"}};
    static constexpr std::array unset{std::string_view{"--unset"}};
    switch (flagType(flag)) {
    case FlagType::State:
        return states;
    case FlagType::Set:
        return sets;
    case FlagType::Boolean:
        return booleans;
    default:
        return unset;
    }
}
std::span<const Flag> supportedFlags()
{
    return flags;
}

std::optional<Flag> parseFlag(std::string_view name)
{
    for (const auto &entry : definitions) {
        if (entry.name == name) {
            return entry.flag;
        }
    }
    return std::nullopt;
}
std::string_view stateName(FlagState state)
{
    for (const auto &[value, name] : state_names) {
        if (value == state) {
            return name;
        }
    }
    throw std::invalid_argument("Unknown region flag state");
}
std::string_view stateName(const FlagValue &value)
{
    if (const auto state = value.state()) {
        return stateName(*state);
    }
    throw std::invalid_argument("A state name requires a state flag value");
}
std::span<const FlagState> supportedFlagStates()
{
    return states;
}
std::optional<FlagState> parseState(std::string_view name)
{
    for (const auto &[value, candidate] : state_names) {
        if (candidate == name) {
            return value;
        }
    }
    return std::nullopt;
}
std::string_view regionGroupName(RegionGroup group)
{
    for (const auto &[value, name] : group_names) {
        if (value == group) {
            return name;
        }
    }
    throw std::invalid_argument("Unknown region group");
}
std::span<const RegionGroup> supportedRegionGroups()
{
    return groups;
}
std::optional<RegionGroup> parseRegionGroup(std::string_view name)
{
    for (const auto &[value, candidate] : group_names) {
        if (candidate == name) {
            return value;
        }
    }
    return std::nullopt;
}
}
