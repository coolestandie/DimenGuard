#include "dimenguard/region/protection_policy.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace dimenguard {

namespace {
std::vector<const Region *> effectiveRegions(std::span<const Region *const> matching, const RegionContext &context,
                                             bool membership = false)
{
    std::unordered_set<const Region *> ancestors;
    for (const auto *region : matching) {
        if (region->kind == RegionKind::Template || (membership && context.isPassthrough(*region))) {
            continue;
        }
        const Region *parent = context.parent(*region);
        for (std::size_t depth = 1; parent && depth < maximum_region_depth; ++depth, parent = context.parent(*parent)) {
            ancestors.insert(parent);
        }
    }
    std::vector<const Region *> effective;
    effective.reserve(matching.size());
    for (const auto *region : matching) {
        if (region->kind != RegionKind::Template && !ancestors.contains(region) &&
            (!membership || !context.isPassthrough(*region))) {
            effective.push_back(region);
        }
    }
    return effective;
}

std::optional<bool> explicitDecision(std::span<const Region *const> matching, Flag flag, std::string_view player_id,
                                     const RegionContext &context, bool has_build_region)
{
    for (std::size_t begin = 0; begin < matching.size();) {
        std::size_t end = begin;
        bool allowed = false;
        while (end < matching.size() && samePriority(*matching[end], *matching[begin])) {
            const auto &region = *matching[end];
            const auto state = context.scopedState(region, flag, player_id);
            // Global build protects wilderness; ordinary regions keep their own membership policy.
            const bool global_build = region.kind == RegionKind::Global && flag == Flag::Build;
            if (state == FlagState::Deny && (!global_build || !has_build_region)) {
                return false;
            }
            allowed = allowed || (state == FlagState::Allow && !global_build);
            ++end;
        }
        if (allowed) {
            return true;
        }
        begin = end;
    }
    return std::nullopt;
}

bool resolve(std::span<const Region *const> matching, std::span<const Region *const> effective, Flag flag,
             std::string_view player_id, const RegionContext &context)
{
    const bool has_build_region = flag == Flag::Build && std::ranges::any_of(matching, [&](const Region *region) {
                                      return region->kind == RegionKind::Cuboid && !context.isPassthrough(*region);
                                  });
    if (const auto decision = explicitDecision(effective, flag, player_id, context, has_build_region)) {
        return *decision;
    }
    if (const auto aggregate = flagFallback(flag)) {
        return resolve(matching, effective, *aggregate, player_id, context);
    }
    const auto fallback = flagDefault(flag);
    if (fallback != FlagDefault::Members) {
        return fallback == FlagDefault::Allow;
    }
    const Region *highest = nullptr;
    for (const auto *region : effectiveRegions(matching, context, true)) {
        if (!highest) {
            highest = region;
        }
        if (!samePriority(*region, *highest)) {
            break;
        }
        if (!context.isMember(*region, player_id)) {
            return false;
        }
    }
    return true;
}

}

bool ProtectionPolicy::isAllowed(std::span<const Region *const> matching, Flag flag, std::string_view player_id,
                                 bool bypass, const RegionContext &context)
{
    static_cast<void>(flagDefault(flag));
    if (bypass) {
        return true;
    }
    return resolve(matching, effectiveRegions(matching, context), flag, player_id, context);
}

bool ProtectionPolicy::isEnvironmentAllowed(std::span<const Region *const> matching, Flag flag,
                                            const RegionContext &context)
{
    if (flagScope(flag) != FlagScope::Environment) {
        throw std::invalid_argument("An environmental decision requires an environmental flag");
    }
    return resolve(matching, effectiveRegions(matching, context), flag, {}, context);
}

}
