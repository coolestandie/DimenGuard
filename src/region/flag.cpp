#include "dimenguard/region/flag.h"

#include <array>
#include <stdexcept>
#include <utility>

namespace dimenguard {
namespace {
struct FlagDefinition {
    Flag flag;
    std::string_view name;
    FlagScope scope;
    FlagDefault fallback;
};
constexpr std::array definitions{
    FlagDefinition{Flag::Build, "build", FlagScope::Player, FlagDefault::Members},
    FlagDefinition{Flag::Interact, "interact", FlagScope::Player, FlagDefault::Members},
    FlagDefinition{Flag::ContainerAccess, "container-access", FlagScope::Player, FlagDefault::Members},
    FlagDefinition{Flag::Pvp, "pvp", FlagScope::Player, FlagDefault::Allow},
    FlagDefinition{Flag::Explosions, "explosions", FlagScope::Environment, FlagDefault::Allow},
    FlagDefinition{Flag::FluidFlow, "fluid-flow", FlagScope::Environment, FlagDefault::Allow},
    FlagDefinition{Flag::BlockForm, "block-form", FlagScope::Environment, FlagDefault::Allow},
    FlagDefinition{Flag::LeafDecay, "leaf-decay", FlagScope::Environment, FlagDefault::Allow},
    FlagDefinition{Flag::ActorGriefing, "actor-griefing", FlagScope::Environment, FlagDefault::Allow},
    FlagDefinition{Flag::MobSpawning, "mob-spawning", FlagScope::Environment, FlagDefault::Allow},
    FlagDefinition{Flag::MobDamage, "mob-damage", FlagScope::Environment, FlagDefault::Allow},
    FlagDefinition{Flag::Entry, "entry", FlagScope::Transition, FlagDefault::Allow},
    FlagDefinition{Flag::Exit, "exit", FlagScope::Transition, FlagDefault::Allow},
};
constexpr std::array state_names{
    std::pair{FlagState::Inherit, std::string_view{"inherit"}},
    std::pair{FlagState::Allow, std::string_view{"allow"}},
    std::pair{FlagState::Deny, std::string_view{"deny"}},
};
constexpr auto flags = [] {
    std::array<Flag, definitions.size()> values{};
    for (std::size_t index = 0; index < values.size(); ++index) {
        values[index] = definitions[index].flag;
    }
    return values;
}();
constexpr auto states = [] {
    std::array<FlagState, state_names.size()> values{};
    for (std::size_t index = 0; index < values.size(); ++index) {
        values[index] = state_names[index].first;
    }
    return values;
}();
const FlagDefinition &definition(Flag flag)
{
    for (const auto &entry : definitions) {
        if (entry.flag == flag) {
            return entry;
        }
    }
    throw std::invalid_argument("Unknown region flag");
}
}
std::string_view flagName(Flag flag)
{
    return definition(flag).name;
}
FlagScope flagScope(Flag flag)
{
    return definition(flag).scope;
}
FlagDefault flagDefault(Flag flag)
{
    return definition(flag).fallback;
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
}
