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
    std::optional<Flag> aggregate = std::nullopt;
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
    FlagDefinition{Flag::BlockBreak, "block-break", FlagScope::Player, FlagDefault::Members, Flag::Build},
    FlagDefinition{Flag::BlockPlace, "block-place", FlagScope::Player, FlagDefault::Members, Flag::Build},
    FlagDefinition{Flag::Use, "use", FlagScope::Player, FlagDefault::Members, Flag::Interact},
    FlagDefinition{Flag::UseAnvil, "use-anvil", FlagScope::Player, FlagDefault::Members, Flag::Use},
    FlagDefinition{Flag::Sleep, "sleep", FlagScope::Player, FlagDefault::Members, Flag::Interact},
    FlagDefinition{Flag::ItemDrop, "item-drop", FlagScope::Player, FlagDefault::Allow},
    FlagDefinition{Flag::ItemPickup, "item-pickup", FlagScope::Environment, FlagDefault::Allow},
    FlagDefinition{Flag::SendChat, "send-chat", FlagScope::Player, FlagDefault::Allow},
    FlagDefinition{Flag::WaterFlow, "water-flow", FlagScope::Environment, FlagDefault::Allow, Flag::FluidFlow},
    FlagDefinition{Flag::LavaFlow, "lava-flow", FlagScope::Environment, FlagDefault::Allow, Flag::FluidFlow},
    FlagDefinition{Flag::FallDamage, "fall-damage", FlagScope::Environment, FlagDefault::Allow},
    FlagDefinition{Flag::FireworkDamage, "firework-damage", FlagScope::Environment, FlagDefault::Allow},
    FlagDefinition{Flag::Invincible, "invincible", FlagScope::Environment, FlagDefault::Deny},
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
const FlagDefinition &definition(Flag flag)
{
    const auto index = static_cast<std::size_t>(flag);
    if (index >= definitions.size()) {
        throw std::invalid_argument("Unknown region flag");
    }
    return definitions[index];
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
std::optional<Flag> flagFallback(Flag flag)
{
    return definition(flag).aggregate;
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
