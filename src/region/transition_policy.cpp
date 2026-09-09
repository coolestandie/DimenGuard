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
                                 std::string_view player_id, const RegionContext &context)
{
    return !getDenial(from, to, player_id, context);
}

std::optional<TransitionDenial> TransitionPolicy::getDenial(std::span<const Region *const> from,
                                                            std::span<const Region *const> to,
                                                            std::string_view player_id, const RegionContext &context)
{
    const auto exited = difference(from, to);
    const auto entered = difference(to, from);
    for (const auto flag : {Flag::Exit, Flag::Entry}) {
        const auto &changed = flag == Flag::Exit ? exited : entered;
        if (!ProtectionPolicy::isAllowed(changed, flag, player_id, false, context)) {
            const auto value = ProtectionPolicy::getFlagValue(
                changed, flag == Flag::Exit ? Flag::ExitDenyMessage : Flag::EntryDenyMessage,
                RegionSubject::player(player_id), context);
            return TransitionDenial{flag, value ? std::optional{*value->get<std::string>()} : std::nullopt};
        }
    }
    return std::nullopt;
}
}
