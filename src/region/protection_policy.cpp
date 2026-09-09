#include "dimenguard/region/protection_policy.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>

namespace dimenguard {

namespace {
std::optional<bool> explicitDecision(std::span<const Region *const> matching, Flag flag)
{
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
    return std::nullopt;
}
}

bool ProtectionPolicy::isAllowed(std::span<const Region *const> matching, Flag flag, std::string_view player_id,
                                 bool bypass)
{
    const auto fallback = flagDefault(flag);
    if (bypass) {
        return true;
    }
    if (const auto decision = explicitDecision(matching, flag)) {
        return *decision;
    }
    if (const auto aggregate = flagFallback(flag)) {
        return isAllowed(matching, *aggregate, player_id);
    }
    if (fallback == FlagDefault::Deny) {
        return false;
    }
    if (matching.empty() || fallback == FlagDefault::Allow) {
        return true;
    }
    const int priority = matching.front()->priority;
    return std::ranges::all_of(
        matching, [&](const Region *region) { return region->priority != priority || region->isMember(player_id); });
}

bool ProtectionPolicy::isEnvironmentAllowed(std::span<const Region *const> matching, Flag flag)
{
    if (flagScope(flag) != FlagScope::Environment) {
        throw std::invalid_argument("An environmental decision requires an environmental flag");
    }
    if (const auto decision = explicitDecision(matching, flag)) {
        return *decision;
    }
    if (const auto aggregate = flagFallback(flag)) {
        return isEnvironmentAllowed(matching, *aggregate);
    }
    return flagDefault(flag) == FlagDefault::Allow;
}

}
