#pragma once

#include "dimenguard/region/region.h"
#include "dimenguard/region/region_subject.h"

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
    [[nodiscard]] bool isMember(const Region &region, const RegionSubject &subject) const;
    [[nodiscard]] bool isOwner(const Region &region, std::string_view player_id) const;
    [[nodiscard]] bool matches(const Region &region, RegionGroup group, std::string_view player_id) const;
    [[nodiscard]] bool matches(const Region &region, RegionGroup group, const RegionSubject &subject) const;
    [[nodiscard]] RegionGroup group(const Region &region, Flag flag) const;
    [[nodiscard]] std::optional<FlagState> scopedState(const Region &region, Flag flag,
                                                       std::string_view player_id) const;
    [[nodiscard]] std::optional<FlagValue> scopedValue(const Region &region, Flag flag,
                                                       const RegionSubject &subject) const;
    [[nodiscard]] bool isPassthrough(const Region &region) const;

private:
    std::span<const Region> regions_;
    std::span<const std::optional<std::size_t>> parents_;
};

}
