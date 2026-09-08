#pragma once

#include "dimenguard/region/region_manager.h"
#include "dimenguard/storage/sqlite_store.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace dimenguard {

enum class ServiceErrorCode {
    NotFound,
    Exists,
    InvalidName,
    LimitReached,
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
    static constexpr std::size_t MaxRegions = 10000;

    explicit RegionService(const std::filesystem::path &path);

    [[nodiscard]] const RegionManager &getRegions() const noexcept;
    void reload();
    void create(Region region);
    void erase(const RegionKey &key);
    void rename(const RegionKey &key, std::string name);
    void setPriority(const RegionKey &key, int priority);
    void setFlag(const RegionKey &key, Flag flag, FlagState state);
    void setMember(const RegionKey &key, std::string player_id, bool trusted);

private:
    void replaceSnapshot(std::vector<Region> regions);
    void mutateRegion(const RegionKey &key, const std::function<void(Region &)> &mutation);

    SqliteStore store_;
    RegionManager regions_;
};

}  // namespace dimenguard
