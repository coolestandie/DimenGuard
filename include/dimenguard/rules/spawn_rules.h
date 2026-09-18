#pragma once

#include "dimenguard/region/region_manager.h"

#include <string_view>

namespace dimenguard {

enum class SpawnSubject {
    Actor,
    Mob,
    Player
};

class SpawnRules {
public:
    [[nodiscard]] static bool isAllowed(const RegionManager &regions, const DimensionKey &dimension,
                                        const BlockPosition &position, std::string_view actor_id, SpawnSubject subject);
};

}
