#pragma once

#include "dimenguard/region/region.h"

#include <span>
#include <string_view>

namespace dimenguard {

enum class RegionSubjectKind {
    Player,
    Environment,
    NonPlayer
};

// Region and domain views are borrowed only for a synchronous query on one validated snapshot.
struct RegionSubject {
    RegionSubjectKind kind = RegionSubjectKind::Environment;
    std::string_view player_id;
    std::span<const Region *const> source_regions;
    const FlagSet *source_domains = nullptr;

    [[nodiscard]] static RegionSubject player(std::string_view identity)
    {
        return {RegionSubjectKind::Player, identity, {}, nullptr};
    }

    [[nodiscard]] static RegionSubject environment() { return {}; }

    [[nodiscard]] static RegionSubject nonPlayer(std::span<const Region *const> regions, const FlagSet &source)
    {
        return {RegionSubjectKind::NonPlayer, {}, regions, &source};
    }
};

}
