#include "dimenguard/region/transition_policy.h"

#include "dimenguard/region/protection_policy.h"

#include <unordered_set>
#include <vector>

namespace dimenguard {
namespace {
std::vector<const Region *> difference(std::span<const Region *const> source, std::span<const Region *const> other)
{
    const std::unordered_set<const Region *> existing(other.begin(), other.end());
    std::vector<const Region *> changed;
    changed.reserve(source.size());
    for (const auto *region : source) {
        if (!existing.contains(region)) {
            changed.push_back(region);
        }
    }
    return changed;
}
}

bool TransitionPolicy::isAllowed(std::span<const Region *const> from, std::span<const Region *const> to,
                                 std::string_view player_id)
{
    const auto exited = difference(from, to);
    const auto entered = difference(to, from);
    return ProtectionPolicy::isAllowed(exited, Flag::Exit, player_id) &&
           ProtectionPolicy::isAllowed(entered, Flag::Entry, player_id);
}
}
