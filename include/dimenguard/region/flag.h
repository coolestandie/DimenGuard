#pragma once

#include "dimenguard/region/flag_value.h"

#include <optional>
#include <span>
#include <string_view>

namespace dimenguard {
enum class Flag {
#define DG_FLAG(id, name, scope, fallback, type, aggregate, description) id,
#include "dimenguard/region/flags.inc"
#undef DG_FLAG
};

enum class FlagScope {
    Player,
    Environment,
    Transition
};
enum class FlagDefault {
    Members,
    Allow,
    Deny,
    None,
    EmptySet
};

enum class FlagConstraint {
    None,
    ActorIds,
    DomainIds,
    PlainText
};

enum class RegionGroup {
    All,
    Members,
    Owners,
    NonMembers,
    NonOwners
};

struct FlagDefinition {
    Flag flag;
    std::string_view name;
    FlagScope scope;
    FlagDefault fallback;
    FlagType type;
    std::optional<Flag> aggregate;
    FlagConstraint constraint;
};

[[nodiscard]] const FlagDefinition &flagDefinition(Flag flag);
[[nodiscard]] std::span<const FlagDefinition> flagDefinitions();
[[nodiscard]] std::string_view flagName(Flag flag);
[[nodiscard]] FlagScope flagScope(Flag flag);
[[nodiscard]] FlagDefault flagDefault(Flag flag);
[[nodiscard]] FlagType flagType(Flag flag);
[[nodiscard]] std::optional<Flag> flagFallback(Flag flag);
[[nodiscard]] std::optional<FlagValue> flagDefaultValue(Flag flag);
[[nodiscard]] std::span<const Flag> supportedFlags();
[[nodiscard]] std::optional<Flag> parseFlag(std::string_view name);
[[nodiscard]] std::optional<FlagValue> parseFlagValue(Flag flag, std::string_view text);
void validateFlagValue(Flag flag, const FlagValue &value);
[[nodiscard]] std::span<const std::string_view> flagValueSuggestions(Flag flag);
[[nodiscard]] std::string_view stateName(FlagState state);
[[nodiscard]] std::string_view stateName(const FlagValue &value);
[[nodiscard]] std::span<const FlagState> supportedFlagStates();
[[nodiscard]] std::optional<FlagState> parseState(std::string_view name);
[[nodiscard]] std::string_view regionGroupName(RegionGroup group);
[[nodiscard]] std::span<const RegionGroup> supportedRegionGroups();
[[nodiscard]] std::optional<RegionGroup> parseRegionGroup(std::string_view name);
}
