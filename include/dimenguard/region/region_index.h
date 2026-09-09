#pragma once

#include "dimenguard/region/region.h"

#include <cstddef>
#include <map>
#include <span>
#include <vector>

namespace dimenguard {

// Indexes validated regions using geometry and vector indices, never borrowed snapshot pointers.
class RegionIndex {
public:
    void replaceAll(std::span<const Region> regions);
    [[nodiscard]] std::vector<std::size_t> query(const DimensionKey &dimension, const BlockPosition &position) const;
    void swap(RegionIndex &other) noexcept;

private:
    struct Entry {
        Bounds bounds;
        std::size_t region_index;
    };

    struct Node {
        Bounds bounds;
        std::size_t begin = 0;
        std::size_t end = 0;
        std::size_t left = 0;
        std::size_t right = 0;
        bool leaf = true;
    };

    struct SpatialTree {
        std::vector<Entry> entries;
        std::vector<Node> nodes;
    };

    static std::size_t buildNode(SpatialTree &tree, std::size_t begin, std::size_t end);
    static void queryNode(const SpatialTree &tree, std::size_t node_index, const BlockPosition &position,
                          std::vector<std::size_t> &result);

    std::map<DimensionKey, SpatialTree> trees_;
};

}
