#pragma once

#include "dimenguard/region/coordinates.h"
#include "dimenguard/region/region.h"

#include <endstone/block/block.h>
#include <endstone/level/dimension.h>
#include <endstone/level/level.h>
#include <endstone/level/location.h>
#include <stdexcept>

namespace dimenguard {

[[nodiscard]] inline DimensionKey dimensionKey(const endstone::Dimension &dimension)
{
    return {dimension.getLevel().getName(), std::string(dimension.getId())};
}

[[nodiscard]] inline BlockPosition blockPosition(const endstone::Location &location)
{
    const auto x = floorBlockCoordinate(location.getX());
    const auto y = floorBlockCoordinate(location.getY());
    const auto z = floorBlockCoordinate(location.getZ());
    if (!x || !y || !z) {
        throw std::invalid_argument("A location has invalid block coordinates");
    }
    return {*x, *y, *z};
}

[[nodiscard]] inline BlockPosition blockPosition(const endstone::Block &block)
{
    return {block.getX(), block.getY(), block.getZ()};
}

}  // namespace dimenguard
