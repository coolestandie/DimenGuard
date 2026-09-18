#include "dimenguard/rules/explosion_rules.h"

#include <array>

namespace dimenguard {

ExplosionSource explosionSource(ExplosionActorType actor)
{
    if (actor.name_space.empty() || actor.key.empty()) {
        return ExplosionSource::Unknown;
    }
    if (actor.name_space == "minecraft") {
        if (actor.key == "tnt" || actor.key == "tnt_minecart") {
            return ExplosionSource::Tnt;
        }
        if (actor.key == "creeper") {
            return ExplosionSource::Creeper;
        }
    }
    return ExplosionSource::Other;
}

std::optional<ExplosionSource> explosionDamageSource(std::string_view cause, std::optional<ExplosionActorType> direct,
                                                     std::optional<ExplosionActorType> responsible)
{
    if (cause != "block_explosion" && cause != "entity_explosion") {
        return std::nullopt;
    }
    if (direct) {
        return explosionSource(*direct);
    }
    if (responsible) {
        const auto source = explosionSource(*responsible);
        if (source == ExplosionSource::Tnt || source == ExplosionSource::Creeper) {
            return source;
        }
    }
    return ExplosionSource::Unknown;
}

std::span<const Flag> explosionFlags(ExplosionSource source)
{
    static constexpr std::array tnt{Flag::Tnt};
    static constexpr std::array creeper{Flag::CreeperExplosion};
    static constexpr std::array other{Flag::OtherExplosion};
    static constexpr std::array unknown{Flag::Tnt, Flag::CreeperExplosion, Flag::OtherExplosion};
    switch (source) {
    case ExplosionSource::Tnt:
        return tnt;
    case ExplosionSource::Creeper:
        return creeper;
    case ExplosionSource::Other:
        return other;
    case ExplosionSource::Unknown:
        return unknown;
    }
    return unknown;
}

}
