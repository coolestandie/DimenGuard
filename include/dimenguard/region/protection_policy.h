#pragma once

#include "dimenguard/region/region.h"

#include <span>
#include <string_view>

namespace dimenguard {

class ProtectionPolicy {
public:
    // Matching regions must be validated, non-null and ordered by descending priority.
    [[nodiscard]] static bool isAllowed(std::span<const Region *const> matching, Flag flag, std::string_view player_id,
                                        bool bypass = false);
    [[nodiscard]] static bool isEnvironmentAllowed(std::span<const Region *const> matching, Flag flag);
};

}
