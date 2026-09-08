#include "dimenguard/region/region_manager.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>

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

}  // namespace

const std::vector<Region> &RegionManager::getAll() const
{
    return regions_;
}

void RegionManager::replaceAll(std::vector<Region> regions)
{
    RegionManager replacement;
    replacement.regions_ = std::move(regions);
    for (std::size_t index = 0; index < replacement.regions_.size(); ++index) {
        const auto &region = replacement.regions_[index];
        validateRegion(region);
        if (!replacement.keys_.emplace(region.key, index).second) {
            throw std::invalid_argument("Duplicate region name within the same level and dimension");
        }
        replacement.trees_[region.key.dimension].entries.push_back(index);
    }
    for (auto &[dimension, tree] : replacement.trees_) {
        replacement.buildNode(tree, 0, tree.entries.size());
    }
    regions_.swap(replacement.regions_);
    keys_.swap(replacement.keys_);
    trees_.swap(replacement.trees_);
}

const Region *RegionManager::find(const RegionKey &key) const
{
    const auto it = keys_.find(key);
    return it == keys_.end() ? nullptr : &regions_[it->second];
}

std::vector<const Region *> RegionManager::query(const DimensionKey &dimension, const BlockPosition &position) const
{
    std::vector<const Region *> result;
    const auto it = trees_.find(dimension);
    if (it == trees_.end()) {
        return result;
    }
    queryNode(it->second, 0, position, result);
    std::ranges::sort(result, [](const Region *a, const Region *b) {
        return a->priority != b->priority ? a->priority > b->priority : a->key.name < b->key.name;
    });
    return result;
}

bool RegionManager::isAllowed(const DimensionKey &dimension, const BlockPosition &position, Flag flag,
                              std::string_view player_id, bool bypass) const
{
    static_cast<void>(flagName(flag));
    if (bypass) {
        return true;
    }
    const auto matching = query(dimension, position);
    for (std::size_t begin = 0; begin < matching.size();) {
        std::size_t end = begin;
        bool allowed = false;
        while (end < matching.size() && matching[end]->priority == matching[begin]->priority) {
            const auto it = matching[end]->flags.find(flag);
            if (it != matching[end]->flags.end()) {
                if (it->second == FlagState::Deny) {
                    return false;
                }
                allowed = allowed || it->second == FlagState::Allow;
            }
            ++end;
        }
        if (allowed) {
            return true;
        }
        begin = end;
    }
    if (matching.empty() || flag == Flag::Pvp) {
        return true;
    }
    const int priority = matching.front()->priority;
    return std::ranges::all_of(
        matching, [&](const Region *region) { return region->priority != priority || region->isMember(player_id); });
}

std::size_t RegionManager::buildNode(SpatialTree &tree, std::size_t begin, std::size_t end)
{
    auto bounds = regions_[tree.entries[begin]].bounds;
    for (std::size_t index = begin + 1; index < end; ++index) {
        bounds = combine(bounds, regions_[tree.entries[index]].bounds);
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
    const auto center = [&](std::size_t index) {
        const auto &region_bounds = regions_[index].bounds;
        return static_cast<std::int64_t>(coordinate(region_bounds.min, axis)) + coordinate(region_bounds.max, axis);
    };
    using Difference = std::vector<std::size_t>::difference_type;
    std::nth_element(
        tree.entries.begin() + static_cast<Difference>(begin), tree.entries.begin() + static_cast<Difference>(middle),
        tree.entries.begin() + static_cast<Difference>(end),
        [&](std::size_t a, std::size_t b) { return center(a) != center(b) ? center(a) < center(b) : a < b; });
    const auto left = buildNode(tree, begin, middle);
    const auto right = buildNode(tree, middle, end);
    tree.nodes[node_index].left = left;
    tree.nodes[node_index].right = right;
    tree.nodes[node_index].leaf = false;
    return node_index;
}

void RegionManager::queryNode(const SpatialTree &tree, std::size_t node_index, const BlockPosition &position,
                              std::vector<const Region *> &result) const
{
    const auto &node = tree.nodes[node_index];
    if (!node.bounds.contains(position)) {
        return;
    }
    if (node.leaf) {
        for (auto index = node.begin; index < node.end; ++index) {
            const auto &region = regions_[tree.entries[index]];
            if (region.bounds.contains(position)) {
                result.push_back(&region);
            }
        }
        return;
    }
    queryNode(tree, node.left, position, result);
    queryNode(tree, node.right, position, result);
}

}  // namespace dimenguard
