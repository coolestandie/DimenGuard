#include "dimenguard/region/protection_policy.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <unordered_set>
#include <utility>
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

std::optional<bool> explicitDecision(std::span<const Region *const> matching, Flag flag, const RegionSubject &subject,
                                     const RegionContext &context, bool has_build_region)
{
    for (std::size_t begin = 0; begin < matching.size();) {
        std::size_t end = begin;
        bool allowed = false;
        while (end < matching.size() && samePriority(*matching[end], *matching[begin])) {
            const auto &region = *matching[end];
            const auto value = context.scopedValue(region, flag, subject);
            const auto state = value ? value->state() : std::nullopt;
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
             const RegionSubject &subject, const RegionContext &context)
{
    const bool has_build_region = flag == Flag::Build && std::ranges::any_of(matching, [&](const Region *region) {
                                      return region->kind == RegionKind::Cuboid && !context.isPassthrough(*region);
                                  });
    if (const auto decision = explicitDecision(effective, flag, subject, context, has_build_region)) {
        return *decision;
    }
    if (const auto aggregate = flagFallback(flag)) {
        return resolve(matching, effective, *aggregate, subject, context);
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
        if (!context.isMember(*region, subject)) {
            return false;
        }
    }
    return true;
}

std::optional<FlagValue> explicitValue(std::span<const Region *const> matching, Flag flag, const RegionSubject &subject,
                                       const RegionContext &context)
{
    for (std::size_t begin = 0; begin < matching.size();) {
        std::size_t end = begin;
        const Region *selected = nullptr;
        std::optional<FlagValue> result;
        FlagSet combined;
        while (end < matching.size() && samePriority(*matching[end], *matching[begin])) {
            const auto &region = *matching[end++];
            if (auto value = context.scopedValue(region, flag, subject)) {
                if (const auto *entries = value->get<FlagSet>()) {
                    combined.insert(entries->begin(), entries->end());
                }
                if (!selected || region.key.name < selected->key.name) {
                    selected = &region;
                    result = std::move(value);
                }
            }
        }
        if (result) {
            return flagType(flag) == FlagType::Set ? FlagValue(std::move(combined)) : result;
        }
        begin = end;
    }
    if (const auto aggregate = flagFallback(flag)) {
        return explicitValue(matching, *aggregate, subject, context);
    }
    return flagDefaultValue(flag);
}

}

bool ProtectionPolicy::isAllowed(std::span<const Region *const> matching, Flag flag, std::string_view player_id,
                                 bool bypass, const RegionContext &context)
{
    if (flagType(flag) != FlagType::State) {
        throw std::invalid_argument("An allow decision requires a state flag");
    }
    if (bypass) {
        return true;
    }
    return isAllowed(matching, flag, RegionSubject::player(player_id), context);
}

bool ProtectionPolicy::isAllowed(std::span<const Region *const> matching, Flag flag, const RegionSubject &subject,
                                 const RegionContext &context)
{
    if (subject.kind == RegionSubjectKind::Environment && flagScope(flag) != FlagScope::Environment) {
        throw std::invalid_argument("An environmental subject requires an environmental flag");
    }
    if (flagType(flag) != FlagType::State) {
        throw std::invalid_argument("An allow decision requires a state flag");
    }
    return resolve(matching, effectiveRegions(matching, context), flag, subject, context);
}

bool ProtectionPolicy::isEnvironmentAllowed(std::span<const Region *const> matching, Flag flag,
                                            const RegionContext &context)
{
    if (flagScope(flag) != FlagScope::Environment) {
        throw std::invalid_argument("An environmental decision requires an environmental flag");
    }
    return isAllowed(matching, flag, RegionSubject::environment(), context);
}

std::optional<FlagValue> ProtectionPolicy::getFlagValue(std::span<const Region *const> matching, Flag flag,
                                                        const RegionSubject &subject, const RegionContext &context)
{
    if (subject.kind == RegionSubjectKind::Environment && flagScope(flag) != FlagScope::Environment) {
        throw std::invalid_argument("An environmental subject requires an environmental flag");
    }
    if (flagType(flag) == FlagType::State) {
        return isAllowed(matching, flag, subject, context) ? FlagState::Allow : FlagState::Deny;
    }
    return explicitValue(effectiveRegions(matching, context), flag, subject, context);
}

}
