#include "dimenguard/region/region_manager.h"

#include "dimenguard/region/protection_policy.h"
#include "dimenguard/region/transition_policy.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dimenguard {

RegionManager &RegionManager::operator=(const RegionManager &other)
{
    if (this != &other) {
        RegionManager replacement(other);
        swap(replacement);
    }
    return *this;
}

RegionManager::RegionManager(RegionManager &&other) noexcept
{
    swap(other);
}

RegionManager &RegionManager::operator=(RegionManager &&other) noexcept
{
    if (this != &other) {
        RegionManager replacement(std::move(other));
        swap(replacement);
    }
    return *this;
}

void RegionManager::swap(RegionManager &other) noexcept
{
    regions_.swap(other.regions_);
    keys_.swap(other.keys_);
    index_.swap(other.index_);
}

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
    }
    replacement.index_.replaceAll(replacement.regions_);
    swap(replacement);
}

const Region *RegionManager::find(const RegionKey &key) const
{
    const auto found = keys_.find(key);
    return found == keys_.end() ? nullptr : &regions_[found->second];
}

std::vector<const Region *> RegionManager::inDimension(const DimensionKey &dimension) const
{
    std::vector<const Region *> result;
    for (auto found = keys_.lower_bound({dimension, ""}); found != keys_.end() && found->first.dimension == dimension;
         ++found) {
        result.push_back(&regions_[found->second]);
    }
    return result;
}

std::vector<const Region *> RegionManager::query(const DimensionKey &dimension, const BlockPosition &position) const
{
    const auto indices = index_.query(dimension, position);
    std::vector<const Region *> result;
    result.reserve(indices.size());
    for (const auto index : indices) {
        result.push_back(&regions_[index]);
    }
    std::ranges::sort(result, [](const Region *a, const Region *b) {
        return a->priority != b->priority ? a->priority > b->priority : a->key.name < b->key.name;
    });
    return result;
}

bool RegionManager::isAllowed(const DimensionKey &dimension, const BlockPosition &position, Flag flag,
                              std::string_view player_id, bool bypass) const
{
    const auto matching = bypass ? std::vector<const Region *>{} : query(dimension, position);
    return ProtectionPolicy::isAllowed(matching, flag, player_id, bypass);
}

bool RegionManager::isEnvironmentAllowed(const DimensionKey &dimension, const BlockPosition &position, Flag flag) const
{
    return ProtectionPolicy::isEnvironmentAllowed(query(dimension, position), flag);
}

bool RegionManager::isTransitionAllowed(const DimensionKey &from_dimension, const BlockPosition &from,
                                        const DimensionKey &to_dimension, const BlockPosition &to,
                                        std::string_view player_id, bool bypass) const
{
    if (bypass || (from_dimension == to_dimension && from == to)) {
        return true;
    }
    return TransitionPolicy::isAllowed(query(from_dimension, from), query(to_dimension, to), player_id);
}

}
