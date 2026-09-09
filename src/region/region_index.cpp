#include "dimenguard/region/region_index.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace dimenguard {
namespace {

constexpr std::size_t leaf_capacity = 8;

Bounds combine(const Bounds &a, const Bounds &b)
{
    return {{std::min(a.min.x, b.min.x), std::min(a.min.y, b.min.y), std::min(a.min.z, b.min.z)},
            {std::max(a.max.x, b.max.x), std::max(a.max.y, b.max.y), std::max(a.max.z, b.max.z)}};
}

int coordinate(const BlockPosition &position, std::size_t axis)
{
    const std::array coordinates{position.x, position.y, position.z};
    return coordinates.at(axis);
}

}

void RegionIndex::replaceAll(std::span<const Region> regions)
{
    RegionIndex replacement;
    for (std::size_t index = 0; index < regions.size(); ++index) {
        if (regions[index].kind == RegionKind::Cuboid) {
            replacement.trees_[regions[index].key.dimension].entries.push_back({regions[index].bounds, index});
        }
    }
    for (auto &[dimension, tree] : replacement.trees_) {
        buildNode(tree, 0, tree.entries.size());
    }
    swap(replacement);
}

std::vector<std::size_t> RegionIndex::query(const DimensionKey &dimension, const BlockPosition &position) const
{
    std::vector<std::size_t> result;
    const auto found = trees_.find(dimension);
    if (found != trees_.end()) {
        queryNode(found->second, 0, position, result);
    }
    return result;
}

void RegionIndex::swap(RegionIndex &other) noexcept
{
    trees_.swap(other.trees_);
}

std::size_t RegionIndex::buildNode(SpatialTree &tree, std::size_t begin, std::size_t end)
{
    auto bounds = tree.entries[begin].bounds;
    for (std::size_t index = begin + 1; index < end; ++index) {
        bounds = combine(bounds, tree.entries[index].bounds);
    }
    const auto node_index = tree.nodes.size();
    tree.nodes.push_back({bounds, begin, end});
    if (end - begin <= leaf_capacity) {
        return node_index;
    }

    // Widen before arithmetic: valid regions can span the entire signed coordinate range.
    std::size_t axis = 0;
    std::int64_t longest = -1;
    for (std::size_t candidate = 0; candidate < 3; ++candidate) {
        const auto length =
            static_cast<std::int64_t>(coordinate(bounds.max, candidate)) - coordinate(bounds.min, candidate);
        if (length > longest) {
            axis = candidate;
            longest = length;
        }
    }
    const auto middle = begin + (end - begin) / 2;
    const auto center = [axis](const Entry &entry) {
        return static_cast<std::int64_t>(coordinate(entry.bounds.min, axis)) + coordinate(entry.bounds.max, axis);
    };
    using Difference = std::vector<Entry>::difference_type;
    std::nth_element(tree.entries.begin() + static_cast<Difference>(begin),
                     tree.entries.begin() + static_cast<Difference>(middle),
                     tree.entries.begin() + static_cast<Difference>(end), [&](const Entry &a, const Entry &b) {
                         return center(a) != center(b) ? center(a) < center(b) : a.region_index < b.region_index;
                     });
    const auto left = buildNode(tree, begin, middle);
    const auto right = buildNode(tree, middle, end);
    tree.nodes[node_index].left = left;
    tree.nodes[node_index].right = right;
    tree.nodes[node_index].leaf = false;
    return node_index;
}

void RegionIndex::queryNode(const SpatialTree &tree, std::size_t node_index, const BlockPosition &position,
                            std::vector<std::size_t> &result)
{
    const auto &node = tree.nodes[node_index];
    if (!node.bounds.contains(position)) {
        return;
    }
    if (node.leaf) {
        for (auto index = node.begin; index < node.end; ++index) {
            const auto &entry = tree.entries[index];
            if (entry.bounds.contains(position)) {
                result.push_back(entry.region_index);
            }
        }
        return;
    }
    queryNode(tree, node.left, position, result);
    queryNode(tree, node.right, position, result);
}

}
