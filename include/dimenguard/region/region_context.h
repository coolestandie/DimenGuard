#pragma once

#include "dimenguard/region/region.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace dimenguard {

// Both spans and queried regions must belong to the same validated snapshot; the manager stores only indices.
class RegionContext {
public:
    RegionContext() = default;
    RegionContext(std::span<const Region> regions, std::span<const std::optional<std::size_t>> parents);

    [[nodiscard]] const Region *parent(const Region &region) const;
    [[nodiscard]] bool isMember(const Region &region, std::string_view player_id) const;
    [[nodiscard]] bool isOwner(const Region &region, std::string_view player_id) const;
    [[nodiscard]] bool matches(const Region &region, RegionGroup group, std::string_view player_id) const;
    [[nodiscard]] RegionGroup group(const Region &region, Flag flag) const;
    [[nodiscard]] std::optional<FlagState> scopedState(const Region &region, Flag flag,
                                                       std::string_view player_id) const;
    [[nodiscard]] bool isPassthrough(const Region &region) const;

private:
    std::span<const Region> regions_;
    std::span<const std::optional<std::size_t>> parents_;
};

}
