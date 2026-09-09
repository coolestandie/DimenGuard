#pragma once

#include "dimenguard/region/flag.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace dimenguard {

enum class ExplosionSource {
    Tnt,
    Creeper,
    Other,
    Unknown
};

struct ExplosionActorType {
    std::string_view name_space;
    std::string_view key;
};

[[nodiscard]] ExplosionSource explosionSource(ExplosionActorType actor);
[[nodiscard]] std::optional<ExplosionSource> explosionDamageSource(
    std::string_view cause, std::optional<ExplosionActorType> direct = std::nullopt,
    std::optional<ExplosionActorType> responsible = std::nullopt);
[[nodiscard]] std::span<const Flag> explosionFlags(ExplosionSource source);

template <typename PermissionQuery>
[[nodiscard]] bool allowsExplosion(ExplosionSource source, PermissionQuery &&query)
{
    return std::ranges::all_of(explosionFlags(source), std::forward<PermissionQuery>(query));
}

template <typename Block, typename PermissionQuery, typename BoundaryQuery>
void filterExplosionBlocks(std::vector<Block> &blocks, ExplosionSource source, PermissionQuery &&query,
                           BoundaryQuery &&boundary_query)
{
    std::erase_if(blocks, [&](const Block &block) {
        return !std::invoke(boundary_query, block) ||
               !allowsExplosion(source, [&](Flag flag) { return std::invoke(query, block, flag); });
    });
}

}
