#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>

namespace dimenguard {

/** Converts a block coordinate using floor division, including below zero. */
[[nodiscard]] inline int chunkCoordinate(int block_coordinate)
{
    return block_coordinate / 16 - (block_coordinate % 16 < 0 ? 1 : 0);
}

/** Adds a relative block offset without signed overflow. */
[[nodiscard]] inline int checkedOffset(int coordinate, int offset)
{
    const auto result = static_cast<std::int64_t>(coordinate) + offset;
    if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max()) {
        throw std::out_of_range("Block coordinate offset exceeds the supported range");
    }
    return static_cast<int>(result);
}

/** Rejects non-finite or out-of-range input before any floating-to-integer conversion. */
[[nodiscard]] inline std::optional<int> floorBlockCoordinate(double coordinate)
{
    if (!std::isfinite(coordinate)) {
        return std::nullopt;
    }
    const auto result = std::floor(coordinate);
    if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }
    return static_cast<int>(result);
}

}  // namespace dimenguard
