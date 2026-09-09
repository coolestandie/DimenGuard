#pragma once

#include "dimenguard/region/region.h"

#include <filesystem>
#include <memory>
#include <vector>

namespace dimenguard::sqlite {
class Connection;
}

namespace dimenguard {

/** Stores complete region snapshots. Call from administrative operations, never protection queries. */
class SqliteStore {
public:
    explicit SqliteStore(const std::filesystem::path &path);
    ~SqliteStore();

    SqliteStore(const SqliteStore &) = delete;
    SqliteStore &operator=(const SqliteStore &) = delete;
    SqliteStore(SqliteStore &&) = delete;
    SqliteStore &operator=(SqliteStore &&) = delete;

    [[nodiscard]] std::vector<Region> load() const;
    void save(const std::vector<Region> &regions);

private:
    std::unique_ptr<sqlite::Connection> database_;
};

}
