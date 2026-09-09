#include "dimenguard/rules/spawn_rules.h"

#include <stdexcept>

namespace dimenguard {

bool SpawnRules::isAllowed(const RegionManager &regions, const DimensionKey &dimension, const BlockPosition &position,
                           std::string_view actor_id, SpawnSubject subject)
{
    if (subject == SpawnSubject::Player) {
        return true;
    }
    if (subject != SpawnSubject::Actor && subject != SpawnSubject::Mob) {
        throw std::invalid_argument("Unknown spawn subject");
    }
    if (subject == SpawnSubject::Mob && !regions.isEnvironmentAllowed(dimension, position, Flag::MobSpawning)) {
        return false;
    }
    const auto value = regions.getFlagValue(dimension, position, Flag::DenySpawn);
    if (!value) {
        return true;
    }
    const auto *denied = value->get<FlagSet>();
    if (!denied) {
        throw std::logic_error("The deny-spawn flag requires a set");
    }
    return !denied->contains(std::string(actor_id));
}

}
