#include "dimenguard/service/region_service.h"

#include <algorithm>
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

}  // namespace

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

void RegionService::reload()
{
    auto candidate = prepareSnapshot(store_.load());
    regions_ = std::move(candidate);
}

void RegionService::create(Region region)
{
    requireValidName(region.key.name);
    if (regions_.find(region.key) != nullptr) {
        throw ServiceError(ServiceErrorCode::Exists, "A region with this name already exists in the dimension.");
    }
    if (regions_.getAll().size() >= MaxRegions) {
        throw ServiceError(ServiceErrorCode::LimitReached, "The region limit has been reached.");
    }
    auto candidate = regions_.getAll();
    candidate.push_back(std::move(region));
    replaceSnapshot(std::move(candidate));
}

void RegionService::erase(const RegionKey &key)
{
    auto candidate = regions_.getAll();
    candidate.erase(findRegion(candidate, key));
    replaceSnapshot(std::move(candidate));
}

void RegionService::rename(const RegionKey &key, std::string name)
{
    requireValidName(name);
    mutateRegion(key, [this, &name](Region &region) {
        const RegionKey renamed{region.key.dimension, name};
        if (renamed != region.key && regions_.find(renamed) != nullptr) {
            throw ServiceError(ServiceErrorCode::Exists, "A region with this name already exists in the dimension.");
        }
        region.key.name = std::move(name);
    });
}

void RegionService::setPriority(const RegionKey &key, int priority)
{
    mutateRegion(key, [priority](Region &region) { region.priority = priority; });
}

void RegionService::setFlag(const RegionKey &key, Flag flag, FlagState state)
{
    static_cast<void>(flagName(flag));
    static_cast<void>(stateName(state));
    mutateRegion(key, [flag, state](Region &region) {
        if (state == FlagState::Inherit) {
            region.flags.erase(flag);
        }
        else {
            region.flags[flag] = state;
        }
    });
}

void RegionService::setMember(const RegionKey &key, std::string player_id, bool trusted)
{
    if (player_id.empty()) {
        throw std::invalid_argument("Region member identities must not be empty.");
    }
    mutateRegion(key, [&player_id, trusted](Region &region) {
        if (trusted) {
            region.members.insert(std::move(player_id));
        }
        else {
            region.members.erase(player_id);
        }
    });
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

}  // namespace dimenguard
