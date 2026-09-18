#pragma once

#include "dimenguard/region/region.h"
#include "dimenguard/region/region_context.h"

#include <span>
#include <string_view>

namespace dimenguard {

class ProtectionPolicy {
public:
    // Matches use snapshot priority order (global last); parented regions require that snapshot's context.
    [[nodiscard]] static bool isAllowed(std::span<const Region *const> matching, Flag flag, std::string_view player_id,
                                        bool bypass = false, const RegionContext &context = {});
    [[nodiscard]] static bool isEnvironmentAllowed(std::span<const Region *const> matching, Flag flag,
                                                   const RegionContext &context = {});
};

}
