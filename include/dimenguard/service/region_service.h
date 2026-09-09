#pragma once

#include "dimenguard/region/region_manager.h"
#include "dimenguard/storage/limits.h"
#include "dimenguard/storage/sqlite_store.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace dimenguard {

enum class ServiceErrorCode {
    NotFound,
    Exists,
    InvalidName,
    LimitReached,
    HasChildren,
    InvalidHierarchy,
    InvalidRegionType,
};

class ServiceError : public std::runtime_error {
public:
    ServiceError(ServiceErrorCode code, const std::string &message);
    [[nodiscard]] ServiceErrorCode getCode() const noexcept;

private:
    ServiceErrorCode code_;
};

/**
 * Owns the live region index and its persistent snapshot on the server's owning thread.
 * Mutations validate and index a candidate before saving; only a committed save replaces live state.
 */
class RegionService {
public:
    static constexpr std::size_t MaxRegions = storage::max_regions;

    explicit RegionService(const std::filesystem::path &path);

    [[nodiscard]] const RegionManager &getRegions() const noexcept;
    [[nodiscard]] std::uint64_t getRegionNamesRevision() const noexcept;
    void reload();
    void create(Region region);
    void createGlobal(DimensionKey dimension, std::string owner);
    void createTemplate(DimensionKey dimension, std::string name, std::string owner);
    void erase(const RegionKey &key);
    void rename(const RegionKey &key, std::string name);
    void setPriority(const RegionKey &key, int priority);
    void setParent(const RegionKey &key, std::optional<RegionKey> parent);
    void setPassthrough(const RegionKey &key, FlagState state);
    void setFlag(const RegionKey &key, Flag flag, FlagState state);
    void setFlagValue(const RegionKey &key, Flag flag, std::optional<FlagValue> value);
    void setFlagGroup(const RegionKey &key, Flag flag, std::optional<RegionGroup> group);
    void setMember(const RegionKey &key, std::string player_id, bool trusted);

private:
    void replaceSnapshot(std::vector<Region> regions);
    void mutateRegion(const RegionKey &key, const std::function<void(Region &)> &mutation);

    SqliteStore store_;
    RegionManager regions_;
    std::uint64_t region_names_revision_ = 0;
};

}
