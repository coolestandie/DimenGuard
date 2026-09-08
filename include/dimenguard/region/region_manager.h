#pragma once

#include "dimenguard/region/region.h"

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
    [[nodiscard]] const std::vector<Region> &getAll() const;

    /** Validates and builds a new index before replacing any live state. */
    void replaceAll(std::vector<Region> regions);

    [[nodiscard]] const Region *find(const RegionKey &key) const;

    /** Returns matching regions by descending priority, then ascending name. */
    [[nodiscard]] std::vector<const Region *> query(const DimensionKey &dimension, const BlockPosition &position) const;

    /**
     * Explicit flags fall through inherited priority tiers; deny wins ties. With no explicit
     * decision, build/interact/container require membership in every top-priority region, while
     * PvP is allowed. No matching region permits the action. Only explicit bypass skips policy.
     */
    [[nodiscard]] bool isAllowed(const DimensionKey &dimension, const BlockPosition &position, Flag flag,
                                 std::string_view player_id, bool bypass = false) const;

private:
    struct Node {
        Bounds bounds;
        std::size_t begin = 0;
        std::size_t end = 0;
        std::size_t left = 0;
        std::size_t right = 0;
        bool leaf = true;
    };

    struct SpatialTree {
        std::vector<std::size_t> entries;
        std::vector<Node> nodes;
    };

    std::size_t buildNode(SpatialTree &tree, std::size_t begin, std::size_t end);
    void queryNode(const SpatialTree &tree, std::size_t node_index, const BlockPosition &position,
                   std::vector<const Region *> &result) const;

    std::vector<Region> regions_;
    std::map<RegionKey, std::size_t> keys_;
    std::map<DimensionKey, SpatialTree> trees_;
};

}  // namespace dimenguard
