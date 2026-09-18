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
    parent_indices_.swap(other.parent_indices_);
    index_.swap(other.index_);
}

const std::vector<Region> &RegionManager::getAll() const
{
    return regions_;
}

void RegionManager::replaceAll(std::vector<Region> regions)
{
    validateRegions(regions);
    RegionManager replacement;
    replacement.regions_ = std::move(regions);
    for (std::size_t index = 0; index < replacement.regions_.size(); ++index) {
        const auto &region = replacement.regions_[index];
        replacement.keys_.emplace(region.key, index);
    }
    replacement.parent_indices_.reserve(replacement.regions_.size());
    for (const auto &region : replacement.regions_) {
        replacement.parent_indices_.push_back(
            region.parent ? std::optional{replacement.keys_.at({region.key.dimension, *region.parent})} : std::nullopt);
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

std::vector<const Region *> RegionManager::overlaps(const DimensionKey &dimension, const Bounds &bounds,
                                                    std::optional<std::string_view> excluded) const
{
    std::vector<const Region *> result;
    for (const auto *region : inDimension(dimension)) {
        if (region->kind != RegionKind::Cuboid || (excluded && region->key.name == *excluded)) {
            continue;
        }
        if (region->bounds.overlaps(bounds)) {
            result.push_back(region);
        }
    }
    std::ranges::sort(result,
                      [](const Region *first, const Region *second) { return first->key.name < second->key.name; });
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
    if (const auto *global = find({dimension, std::string(global_region_name)});
        global && global->kind == RegionKind::Global) {
        result.push_back(global);
    }
    std::ranges::sort(result, [](const Region *a, const Region *b) {
        return samePriority(*a, *b) ? a->key.name < b->key.name : higherPriority(*a, *b);
    });
    return result;
}

bool RegionManager::isAllowed(const DimensionKey &dimension, const BlockPosition &position, Flag flag,
                              std::string_view player_id, bool bypass) const
{
    const auto matching = bypass ? std::vector<const Region *>{} : query(dimension, position);
    return ProtectionPolicy::isAllowed(matching, flag, player_id, bypass, {regions_, parent_indices_});
}

bool RegionManager::isEnvironmentAllowed(const DimensionKey &dimension, const BlockPosition &position, Flag flag) const
{
    return ProtectionPolicy::isEnvironmentAllowed(query(dimension, position), flag, {regions_, parent_indices_});
}

bool RegionManager::isTransitionAllowed(const DimensionKey &from_dimension, const BlockPosition &from,
                                        const DimensionKey &to_dimension, const BlockPosition &to,
                                        std::string_view player_id, bool bypass) const
{
    return !getTransitionDenial(from_dimension, from, to_dimension, to, player_id, bypass);
}

std::optional<TransitionDenial> RegionManager::getTransitionDenial(const DimensionKey &from_dimension,
                                                                   const BlockPosition &from,
                                                                   const DimensionKey &to_dimension,
                                                                   const BlockPosition &to, std::string_view player_id,
                                                                   bool bypass) const
{
    if (bypass || (from_dimension == to_dimension && from == to)) {
        return std::nullopt;
    }
    return TransitionPolicy::getDenial(query(from_dimension, from), query(to_dimension, to), player_id,
                                       {regions_, parent_indices_});
}

std::optional<FlagValue> RegionManager::getFlagValue(const DimensionKey &dimension, const BlockPosition &position,
                                                     Flag flag, std::optional<std::string_view> player_id) const
{
    return ProtectionPolicy::getFlagValue(query(dimension, position), flag,
                                          player_id ? RegionSubject::player(*player_id) : RegionSubject::environment(),
                                          {regions_, parent_indices_});
}

bool RegionManager::isNonPlayerAllowed(const DimensionKey &source_dimension, std::optional<BlockPosition> source,
                                       const DimensionKey &target_dimension, const BlockPosition &target,
                                       Flag flag) const
{
    if (flagScope(flag) != FlagScope::Player || flagType(flag) != FlagType::State) {
        throw std::invalid_argument("Non-player association requires a player-action state flag");
    }
    if (source_dimension != target_dimension) {
        return false;
    }
    auto source_regions = source ? query(source_dimension, *source) : std::vector<const Region *>{};
    const auto target_regions = query(target_dimension, target);
    const auto is_global = [](const Region *region) {
        return region->kind == RegionKind::Global;
    };
    std::erase_if(source_regions, is_global);
    const RegionContext context{regions_, parent_indices_};
    const auto source_domains = ProtectionPolicy::getFlagValue(source_regions, Flag::NonPlayerProtectionDomains,
                                                               RegionSubject::environment(), context);
    const auto subject = RegionSubject::nonPlayer(source_regions, *source_domains->get<FlagSet>());
    return ProtectionPolicy::isAllowed(target_regions, flag, subject, context);
}

}
