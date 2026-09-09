#pragma once

#include <optional>
#include <span>
#include <string_view>

namespace dimenguard {
enum class Flag {
    Build,
    Interact,
    ContainerAccess,
    Pvp,
    Explosions,
    FluidFlow,
    BlockForm,
    LeafDecay,
    ActorGriefing,
    MobSpawning,
    MobDamage,
    Entry,
    Exit,
    BlockBreak,
    BlockPlace,
    Use,
    UseAnvil,
    Sleep,
    ItemDrop,
    ItemPickup,
    SendChat,
    WaterFlow,
    LavaFlow,
    FallDamage,
    FireworkDamage,
    Invincible,
};

enum class FlagState {
    Inherit,
    Allow,
    Deny
};
enum class FlagScope {
    Player,
    Environment,
    Transition
};
enum class FlagDefault {
    Members,
    Allow,
    Deny
};

[[nodiscard]] std::string_view flagName(Flag flag);
[[nodiscard]] FlagScope flagScope(Flag flag);
[[nodiscard]] FlagDefault flagDefault(Flag flag);
[[nodiscard]] std::optional<Flag> flagFallback(Flag flag);
[[nodiscard]] std::span<const Flag> supportedFlags();
[[nodiscard]] std::optional<Flag> parseFlag(std::string_view name);
[[nodiscard]] std::string_view stateName(FlagState state);
[[nodiscard]] std::span<const FlagState> supportedFlagStates();
[[nodiscard]] std::optional<FlagState> parseState(std::string_view name);
}
