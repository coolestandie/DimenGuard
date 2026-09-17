#include "dimenguard/service/region_service.h"

#include <cstdint>
#include <limits>

namespace dimenguard {
namespace {

std::optional<int> addCoordinate(int value, int offset)
{
    const auto result = static_cast<std::int64_t>(value) + static_cast<std::int64_t>(offset);
    if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }
    return static_cast<int>(result);
}

}

void RegionService::setBounds(const RegionKey &key, Bounds bounds)
{
    if (bounds.min.x > bounds.max.x || bounds.min.y > bounds.max.y || bounds.min.z > bounds.max.z) {
        throw ServiceError(ServiceErrorCode::InvalidBounds, "Region bounds must be ordered on every axis.");
    }
    mutateRegion(key, [bounds](Region &region) {
        if (region.kind != RegionKind::Cuboid) {
            throw ServiceError(ServiceErrorCode::InvalidRegionType, "Only cuboid regions have physical bounds.");
        }
        region.bounds = bounds;
    });
}

void RegionService::move(const RegionKey &key, BlockPosition offset)
{
    const auto *region = regions_.find(key);
    if (region == nullptr) {
        throw ServiceError(ServiceErrorCode::NotFound, "The region does not exist.");
    }
    if (region->kind != RegionKind::Cuboid) {
        throw ServiceError(ServiceErrorCode::InvalidRegionType, "Only cuboid regions can be moved.");
    }
    const auto min_x = addCoordinate(region->bounds.min.x, offset.x);
    const auto min_y = addCoordinate(region->bounds.min.y, offset.y);
    const auto min_z = addCoordinate(region->bounds.min.z, offset.z);
    const auto max_x = addCoordinate(region->bounds.max.x, offset.x);
    const auto max_y = addCoordinate(region->bounds.max.y, offset.y);
    const auto max_z = addCoordinate(region->bounds.max.z, offset.z);
    if (!min_x || !min_y || !min_z || !max_x || !max_y || !max_z) {
        throw ServiceError(ServiceErrorCode::InvalidBounds, "Moving the region would exceed block coordinates.");
    }
    setBounds(key, {{*min_x, *min_y, *min_z}, {*max_x, *max_y, *max_z}});
}

}
