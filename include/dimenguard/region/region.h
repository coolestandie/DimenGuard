#pragma once

#include "dimenguard/region/flag.h"

#include <compare>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>

namespace dimenguard {

struct BlockPosition {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const BlockPosition &) const = default;
};

struct DimensionKey {
    std::string level;
    std::string dimension;

    auto operator<=>(const DimensionKey &) const = default;
};

struct RegionKey {
    DimensionKey dimension;
    std::string name;

    auto operator<=>(const RegionKey &) const = default;
};

/** Inclusive block coordinates; even a single selected block is a valid region. */
struct Bounds {
    BlockPosition min;
    BlockPosition max;

    [[nodiscard]] static Bounds between(const BlockPosition &a, const BlockPosition &b);
    [[nodiscard]] bool contains(const BlockPosition &position) const;
    bool operator==(const Bounds &) const = default;
};

[[nodiscard]] bool isValidRegionName(std::string_view name);

struct Region {
    RegionKey key;
    Bounds bounds;
    int priority = 0;
    std::string owner;
    std::unordered_set<std::string> members;
    std::map<Flag, FlagState> flags;

    [[nodiscard]] bool isMember(std::string_view player_id) const;
};

/** Throws std::invalid_argument for invalid identities, bounds, names or flag values. */
void validateRegion(const Region &region);

}
