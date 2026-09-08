#pragma once

#include "dimenguard/region/region.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace dimenguard {

enum class SelectionCorner {
    First,
    Second
};

class SelectionManager {
public:
    void set(const std::string &player_id, const DimensionKey &dimension, BlockPosition position,
             SelectionCorner corner)
    {
        auto &selection = selections_[player_id];
        if (selection.dimension != dimension) {
            selection = {dimension, {}, {}};
        }
        (corner == SelectionCorner::First ? selection.first : selection.second) = position;
    }

    [[nodiscard]] std::optional<Bounds> get(const std::string &player_id, const DimensionKey &dimension) const
    {
        const auto found = selections_.find(player_id);
        if (found == selections_.end()) {
            return std::nullopt;
        }
        const auto &selection = found->second;
        if (selection.dimension != dimension || !selection.first || !selection.second) {
            return std::nullopt;
        }
        return Bounds::between(*selection.first, *selection.second);
    }

    void forget(const std::string &player_id) { selections_.erase(player_id); }

private:
    struct Selection {
        DimensionKey dimension;
        std::optional<BlockPosition> first;
        std::optional<BlockPosition> second;
    };
    std::unordered_map<std::string, Selection> selections_;
};

}  // namespace dimenguard
