#include "dimenguard/service/region_service.h"

#include <utility>

namespace dimenguard {

void RegionService::setPassthrough(const RegionKey &key, FlagState state)
{
    static_cast<void>(stateName(state));
    mutateRegion(key, [state](Region &region) { region.passthrough = state; });
}

void RegionService::setFlag(const RegionKey &key, Flag flag, FlagState state)
{
    static_cast<void>(flagName(flag));
    static_cast<void>(stateName(state));
    mutateRegion(key, [flag, state](Region &region) {
        if (state == FlagState::Inherit) {
            region.flags.erase(flag);
        }
        else {
            region.flags[flag] = state;
        }
    });
}

void RegionService::setFlagGroup(const RegionKey &key, Flag flag, std::optional<RegionGroup> group)
{
    static_cast<void>(flagName(flag));
    if (group) {
        static_cast<void>(regionGroupName(*group));
    }
    mutateRegion(key, [flag, group](Region &region) {
        if (group) {
            region.flag_groups[flag] = *group;
        }
        else {
            region.flag_groups.erase(flag);
        }
    });
}

void RegionService::setMember(const RegionKey &key, std::string player_id, bool trusted)
{
    if (player_id.empty()) {
        throw std::invalid_argument("Region member identities must not be empty.");
    }
    mutateRegion(key, [&player_id, trusted](Region &region) {
        if (trusted) {
            region.members.insert(std::move(player_id));
        }
        else {
            region.members.erase(player_id);
        }
    });
}

}
