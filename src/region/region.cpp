#include "dimenguard/region/region.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace dimenguard {
namespace {
constexpr std::array kind_names{
    std::pair{RegionKind::Cuboid, std::string_view{"cuboid"}},
    std::pair{RegionKind::Global, std::string_view{"global"}},
    std::pair{RegionKind::Template, std::string_view{"template"}},
};
}

std::string_view regionKindName(RegionKind kind)
{
    for (const auto &[value, name] : kind_names) {
        if (value == kind) {
            return name;
        }
    }
    throw std::invalid_argument("Unknown region kind");
}

std::optional<RegionKind> parseRegionKind(std::string_view name)
{
    for (const auto &[value, candidate] : kind_names) {
        if (candidate == name) {
            return value;
        }
    }
    return std::nullopt;
}

bool samePriority(const Region &a, const Region &b)
{
    return a.priority == b.priority && (a.kind == RegionKind::Global) == (b.kind == RegionKind::Global);
}

bool higherPriority(const Region &a, const Region &b)
{
    if ((a.kind == RegionKind::Global) != (b.kind == RegionKind::Global)) {
        return b.kind == RegionKind::Global;
    }
    return a.priority > b.priority;
}

Bounds Bounds::between(const BlockPosition &a, const BlockPosition &b)
{
    return {{std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)},
            {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}};
}

bool Bounds::contains(const BlockPosition &position) const
{
    return position.x >= min.x && position.x <= max.x && position.y >= min.y && position.y <= max.y &&
           position.z >= min.z && position.z <= max.z;
}

bool Region::isMember(std::string_view player_id) const
{
    return player_id == owner || members.contains(std::string(player_id));
}

bool isValidRegionName(std::string_view name)
{
    return !name.empty() && name.size() <= 64 && std::ranges::all_of(name, [](char character) {
        return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '_' ||
               character == '-';
    });
}

void validateRegion(const Region &region)
{
    static_cast<void>(regionKindName(region.kind));
    static_cast<void>(stateName(region.passthrough));
    if (region.key.dimension.level.empty() || region.key.dimension.dimension.empty() || region.owner.empty()) {
        throw std::invalid_argument("Region level, dimension and owner must not be empty");
    }
    if (!isValidRegionName(region.key.name)) {
        throw std::invalid_argument("Region names must contain 1-64 lowercase letters, digits, underscores or hyphens");
    }
    if (region.kind == RegionKind::Global && (region.key.name != global_region_name ||
                                              region.priority != std::numeric_limits<int>::min() || region.parent)) {
        throw std::invalid_argument("Global regions require the reserved name, minimum priority and no parent");
    }
    if (region.parent && (!isValidRegionName(*region.parent) || *region.parent == region.key.name)) {
        throw std::invalid_argument("Region parents must have a valid, different name");
    }
    const auto &bounds = region.bounds;
    if (bounds.min.x > bounds.max.x || bounds.min.y > bounds.max.y || bounds.min.z > bounds.max.z) {
        throw std::invalid_argument("Region bounds must be ordered on every axis");
    }
    if (region.members.contains("")) {
        throw std::invalid_argument("Region member identities must not be empty");
    }
    for (const auto &[flag, state] : region.flags) {
        static_cast<void>(flagName(flag));
        static_cast<void>(stateName(state));
    }
    for (const auto &[flag, group] : region.flag_groups) {
        static_cast<void>(regionGroupName(group));
        if (flagScope(flag) == FlagScope::Environment && group != RegionGroup::All) {
            throw std::invalid_argument("Environmental flags only support the all region group");
        }
    }
}

void validateRegions(std::span<const Region> regions)
{
    std::map<RegionKey, const Region *> indexed;
    for (const auto &region : regions) {
        validateRegion(region);
        if (!indexed.emplace(region.key, &region).second) {
            throw std::invalid_argument("Duplicate region name within the same level and dimension");
        }
    }
    for (const auto &region : regions) {
        const Region *current = &region;
        std::array<const Region *, maximum_region_depth> chain{};
        std::size_t depth = 0;
        while (current) {
            if (std::find(chain.begin(), chain.begin() + static_cast<std::ptrdiff_t>(depth), current) !=
                chain.begin() + static_cast<std::ptrdiff_t>(depth)) {
                throw std::invalid_argument("Region parent hierarchy must not contain cycles");
            }
            if (depth == maximum_region_depth) {
                throw std::invalid_argument("Region parent hierarchy exceeds the maximum depth");
            }
            chain[depth++] = current;
            if (!current->parent) {
                break;
            }
            const auto found = indexed.find({current->key.dimension, *current->parent});
            if (found == indexed.end()) {
                throw std::invalid_argument("Region parent must exist in the same level and dimension");
            }
            if (found->second->kind == RegionKind::Global) {
                throw std::invalid_argument("Global regions are dimension fallbacks and cannot be parents");
            }
            if (higherPriority(*found->second, *current)) {
                throw std::invalid_argument("Region parent priority must not exceed child priority");
            }
            current = found->second;
        }
    }
}

}
