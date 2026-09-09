#pragma once

#include "dimenguard/region/region.h"
#include "dimenguard/region/region_index.h"

#include <cstddef>
#include <map>
#include <string_view>
#include <vector>

namespace dimenguard {

/**
 * Owns validated regions and a bounding-volume tree per dimension.
 *
 * Storage is proportional to region count, including for world-sized cuboids. Queries prune
 * disjoint branches; genuinely overlapping regions still require examination. The manager is
 * confined to its owning thread. Returned references and pointers expire on replaceAll().
 */
class RegionManager {
public:
    RegionManager() = default;
    RegionManager(const RegionManager &) = default;
    RegionManager &operator=(const RegionManager &other);
    RegionManager(RegionManager &&other) noexcept;
    RegionManager &operator=(RegionManager &&other) noexcept;

    [[nodiscard]] const std::vector<Region> &getAll() const;

    /** Validates and builds a new index before replacing any live state. */
    void replaceAll(std::vector<Region> regions);

    [[nodiscard]] const Region *find(const RegionKey &key) const;
    [[nodiscard]] std::vector<const Region *> inDimension(const DimensionKey &dimension) const;

    /** Returns matching regions by descending priority, then ascending name. */
    [[nodiscard]] std::vector<const Region *> query(const DimensionKey &dimension, const BlockPosition &position) const;

    /**
     * Explicit flags fall through inherited priority tiers; deny wins ties. Unset granular flags
     * follow their registered base. Defaults may require membership in every top-priority region;
     * invincible instead defaults to false, including outside regions. Explicit bypass skips this
     * player policy; immunity must be queried through the bypass-independent environmental policy.
     */
    [[nodiscard]] bool isAllowed(const DimensionKey &dimension, const BlockPosition &position, Flag flag,
                                 std::string_view player_id, bool bypass = false) const;
    [[nodiscard]] bool isEnvironmentAllowed(const DimensionKey &dimension, const BlockPosition &position,
                                            Flag flag) const;
    [[nodiscard]] bool isTransitionAllowed(const DimensionKey &from_dimension, const BlockPosition &from,
                                           const DimensionKey &to_dimension, const BlockPosition &to,
                                           std::string_view player_id, bool bypass = false) const;

private:
    void swap(RegionManager &other) noexcept;

    std::vector<Region> regions_;
    std::map<RegionKey, std::size_t> keys_;
    RegionIndex index_;
};

}
