#include "dimenguard/service/region_service.h"

namespace dimenguard {

void RegionService::setPriority(const RegionKey &key, int priority)
{
    try {
        mutateRegion(key, [priority](Region &region) {
            if (region.kind == RegionKind::Global) {
                throw ServiceError(ServiceErrorCode::InvalidRegionType, "The global priority is fixed.");
            }
            region.priority = priority;
        });
    }
    catch (const std::invalid_argument &error) {
        throw ServiceError(ServiceErrorCode::InvalidHierarchy, error.what());
    }
}

void RegionService::setParent(const RegionKey &key, std::optional<RegionKey> parent)
{
    if (parent && parent->dimension != key.dimension) {
        throw ServiceError(ServiceErrorCode::InvalidHierarchy, "A parent must belong to the same level and dimension.");
    }
    try {
        mutateRegion(key, [&parent](Region &region) {
            region.parent = parent ? std::optional<std::string>{parent->name} : std::nullopt;
        });
    }
    catch (const std::invalid_argument &error) {
        throw ServiceError(ServiceErrorCode::InvalidHierarchy, error.what());
    }
}

}
