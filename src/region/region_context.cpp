#include "dimenguard/region/region_context.h"

#include <cstdint>
#include <stdexcept>

namespace dimenguard {

RegionContext::RegionContext(std::span<const Region> regions, std::span<const std::optional<std::size_t>> parents)
    : regions_(regions), parents_(parents)
{
    if (regions_.size() != parents_.size()) {
        throw std::invalid_argument("Region hierarchy indices must match their snapshot size");
    }
}

const Region *RegionContext::parent(const Region &region) const
{
    if (!region.parent) {
        return nullptr;
    }
    if (regions_.empty()) {
        throw std::invalid_argument("Parented regions require their validated snapshot context");
    }
    // Check offsets before indexing without subtracting potentially unrelated snapshot pointers.
    const auto address = reinterpret_cast<std::uintptr_t>(&region);
    const auto begin = reinterpret_cast<std::uintptr_t>(regions_.data());
    if (address < begin || (address - begin) % sizeof(Region) != 0 ||
        (address - begin) / sizeof(Region) >= regions_.size()) {
        throw std::invalid_argument("Region hierarchy context does not match its snapshot");
    }
    const auto index = static_cast<std::size_t>((address - begin) / sizeof(Region));
    const auto parent_index = parents_[index];
    if (!parent_index || *parent_index >= regions_.size()) {
        throw std::invalid_argument("Region hierarchy parent index does not match its snapshot");
    }
    return &regions_[*parent_index];
}

bool RegionContext::isMember(const Region &region, std::string_view player_id) const
{
    if (player_id.empty()) {
        return false;
    }
    const Region *current = &region;
    for (std::size_t depth = 0; current && depth < maximum_region_depth; ++depth, current = parent(*current)) {
        if (current->isMember(player_id)) {
            return true;
        }
    }
    return false;
}

bool RegionContext::isOwner(const Region &region, std::string_view player_id) const
{
    if (player_id.empty()) {
        return false;
    }
    const Region *current = &region;
    for (std::size_t depth = 0; current && depth < maximum_region_depth; ++depth, current = parent(*current)) {
        if (current->owner == player_id) {
            return true;
        }
    }
    return false;
}

bool RegionContext::isMember(const Region &region, const RegionSubject &subject) const
{
    if (subject.kind == RegionSubjectKind::Player) {
        return isMember(region, subject.player_id);
    }
    if (subject.kind != RegionSubjectKind::NonPlayer || region.kind == RegionKind::Global) {
        return false;
    }
    for (const auto *source : subject.source_regions) {
        if (source->kind == RegionKind::Global || source->key.dimension != region.key.dimension) {
            continue;
        }
        const Region *target = &region;
        for (std::size_t target_depth = 0; target && target_depth < maximum_region_depth;
             ++target_depth, target = parent(*target)) {
            const Region *current = source;
            for (std::size_t source_depth = 0; current && source_depth < maximum_region_depth;
                 ++source_depth, current = parent(*current)) {
                if (current == target) {
                    return true;
                }
            }
        }
    }
    if (subject.source_domains && !subject.source_domains->empty()) {
        if (const auto target_domains =
                scopedValue(region, Flag::NonPlayerProtectionDomains, RegionSubject::environment())) {
            for (const auto &domain : *subject.source_domains) {
                if (target_domains->get<FlagSet>()->contains(domain)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool RegionContext::matches(const Region &region, RegionGroup group, std::string_view player_id) const
{
    return matches(region, group, RegionSubject::player(player_id));
}

bool RegionContext::matches(const Region &region, RegionGroup group, const RegionSubject &subject) const
{
    if (subject.kind == RegionSubjectKind::Environment) {
        return group == RegionGroup::All;
    }
    const bool owner = subject.kind == RegionSubjectKind::Player && isOwner(region, subject.player_id);
    switch (group) {
    case RegionGroup::All:
        return true;
    case RegionGroup::Members:
        return isMember(region, subject);
    case RegionGroup::Owners:
        return owner;
    case RegionGroup::NonMembers:
        return !isMember(region, subject);
    case RegionGroup::NonOwners:
        return !owner;
    }
    throw std::invalid_argument("Unknown region group");
}

RegionGroup RegionContext::group(const Region &region, Flag flag) const
{
    const Region *current = &region;
    for (std::size_t depth = 0; current && depth < maximum_region_depth; ++depth, current = parent(*current)) {
        if (const auto found = current->flag_groups.find(flag); found != current->flag_groups.end()) {
            return found->second;
        }
    }
    return RegionGroup::All;
}

std::optional<FlagState> RegionContext::scopedState(const Region &region, Flag flag, std::string_view player_id) const
{
    const auto value = scopedValue(region, flag, RegionSubject::player(player_id));
    return value ? value->state() : std::nullopt;
}

std::optional<FlagValue> RegionContext::scopedValue(const Region &region, Flag flag, const RegionSubject &subject) const
{
    const Region *current = &region;
    const Region *group_source = current;
    for (std::size_t depth = 0; current && depth < maximum_region_depth; ++depth, current = parent(*current)) {
        const auto found = current->flags.find(flag);
        if (found != current->flags.end() && found->second != FlagState::Inherit) {
            if (matches(region, group(*group_source, flag), subject)) {
                return found->second;
            }
            // A scoped exception cannot discard the rules of ancestors outside that exception's scope.
            group_source = parent(*current);
        }
    }
    return std::nullopt;
}

bool RegionContext::isPassthrough(const Region &region) const
{
    const Region *current = &region;
    for (std::size_t depth = 0; current && depth < maximum_region_depth; ++depth, current = parent(*current)) {
        if (current->passthrough != FlagState::Inherit) {
            return current->passthrough == FlagState::Allow;
        }
    }
    return region.kind == RegionKind::Global;
}

}
