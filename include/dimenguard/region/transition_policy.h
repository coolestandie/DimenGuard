#pragma once

#include "dimenguard/region/region.h"

#include <span>
#include <string_view>

namespace dimenguard {
class TransitionPolicy {
public:
    // Both views must come from the same live snapshot and retain descending priority order.
    [[nodiscard]] static bool isAllowed(std::span<const Region *const> from, std::span<const Region *const> to,
                                        std::string_view player_id);
};
}
