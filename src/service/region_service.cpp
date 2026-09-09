#include "dimenguard/service/region_service.h"

#include <algorithm>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

namespace dimenguard {
namespace {

static_assert(std::is_nothrow_move_assignable_v<RegionManager>);

RegionManager prepareSnapshot(std::vector<Region> regions)
{
    if (regions.size() > RegionService::MaxRegions) {
        throw ServiceError(ServiceErrorCode::LimitReached, "The region limit has been reached.");
    }
    RegionManager candidate;
    candidate.replaceAll(std::move(regions));
    return candidate;
}

void requireValidName(std::string_view name)
{
    if (!isValidRegionName(name)) {
        throw ServiceError(ServiceErrorCode::InvalidName, "The region name is invalid.");
    }
}

std::vector<Region>::iterator findRegion(std::vector<Region> &regions, const RegionKey &key)
{
    const auto found = std::ranges::find(regions, key, &Region::key);
    if (found == regions.end()) {
        throw ServiceError(ServiceErrorCode::NotFound, "The region does not exist.");
    }
    return found;
}

}

ServiceError::ServiceError(ServiceErrorCode code, const std::string &message) : std::runtime_error(message), code_(code)
{
}

ServiceErrorCode ServiceError::getCode() const noexcept
{
    return code_;
}

RegionService::RegionService(const std::filesystem::path &path) : store_(path)
{
    reload();
}

const RegionManager &RegionService::getRegions() const noexcept
{
    return regions_;
}

std::uint64_t RegionService::getRegionNamesRevision() const noexcept
{
    return region_names_revision_;
}

void RegionService::reload()
{
    try {
        auto candidate = prepareSnapshot(store_.load());
        regions_ = std::move(candidate);
        ++region_names_revision_;
    }
    catch (const storage::SnapshotLimitError &error) {
        throw ServiceError(ServiceErrorCode::LimitReached, error.what());
    }
}

void RegionService::create(Region region)
{
    requireValidName(region.key.name);
    if (region.key.name == global_region_name && region.kind != RegionKind::Global) {
        throw ServiceError(ServiceErrorCode::InvalidName, "This name is reserved for the dimension's global region.");
    }
    if (regions_.find(region.key) != nullptr) {
        throw ServiceError(ServiceErrorCode::Exists, "A region with this name already exists in the dimension.");
    }
    if (regions_.getAll().size() >= MaxRegions) {
        throw ServiceError(ServiceErrorCode::LimitReached, "The region limit has been reached.");
    }
    auto candidate = regions_.getAll();
    candidate.push_back(std::move(region));
    replaceSnapshot(std::move(candidate));
    ++region_names_revision_;
}

void RegionService::createGlobal(DimensionKey dimension, std::string owner)
{
    Region region;
    region.key = {std::move(dimension), std::string(global_region_name)};
    region.owner = std::move(owner);
    region.kind = RegionKind::Global;
    region.priority = std::numeric_limits<int>::min();
    create(std::move(region));
}

void RegionService::createTemplate(DimensionKey dimension, std::string name, std::string owner)
{
    Region region;
    region.key = {std::move(dimension), std::move(name)};
    region.owner = std::move(owner);
    region.kind = RegionKind::Template;
    create(std::move(region));
}

void RegionService::erase(const RegionKey &key)
{
    auto candidate = regions_.getAll();
    const auto found = findRegion(candidate, key);
    if (std::ranges::any_of(candidate, [&](const Region &region) {
            return region.key.dimension == key.dimension && region.parent == key.name;
        })) {
        throw ServiceError(ServiceErrorCode::HasChildren, "Detach this region's children before deleting it.");
    }
    candidate.erase(found);
    replaceSnapshot(std::move(candidate));
    ++region_names_revision_;
}

void RegionService::rename(const RegionKey &key, std::string name)
{
    requireValidName(name);
    auto candidate = regions_.getAll();
    auto &region = *findRegion(candidate, key);
    if (region.kind == RegionKind::Global) {
        throw ServiceError(ServiceErrorCode::InvalidRegionType, "The global region cannot be renamed.");
    }
    if (name == global_region_name && name != key.name) {
        throw ServiceError(ServiceErrorCode::InvalidName, "This name is reserved for the dimension's global region.");
    }
    const RegionKey renamed{key.dimension, name};
    if (renamed != key && regions_.find(renamed) != nullptr) {
        throw ServiceError(ServiceErrorCode::Exists, "A region with this name already exists in the dimension.");
    }
    region.key.name = name;
    for (auto &child : candidate) {
        if (child.key.dimension == key.dimension && child.parent == key.name) {
            child.parent = name;
        }
    }
    replaceSnapshot(std::move(candidate));
    ++region_names_revision_;
}

void RegionService::replaceSnapshot(std::vector<Region> regions)
{
    auto candidate = prepareSnapshot(std::move(regions));
    store_.save(candidate.getAll());
    regions_ = std::move(candidate);
}

void RegionService::mutateRegion(const RegionKey &key, const std::function<void(Region &)> &mutation)
{
    auto candidate = regions_.getAll();
    mutation(*findRegion(candidate, key));
    replaceSnapshot(std::move(candidate));
}

}
