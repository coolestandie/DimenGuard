#pragma once

#include "dimenguard/region/region.h"
#include "dimenguard/region/region_context.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace dimenguard {
struct TransitionDenial {
    Flag flag;
    std::optional<std::string> message;
};

class TransitionPolicy {
public:
    // Both views must come from the same live snapshot and retain descending priority order.
    [[nodiscard]] static bool isAllowed(std::span<const Region *const> from, std::span<const Region *const> to,
                                        std::string_view player_id, const RegionContext &context = {});
    [[nodiscard]] static std::optional<TransitionDenial> getDenial(std::span<const Region *const> from,
                                                                   std::span<const Region *const> to,
                                                                   std::string_view player_id,
                                                                   const RegionContext &context = {});
};
}
