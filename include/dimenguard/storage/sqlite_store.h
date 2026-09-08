#pragma once

#include "dimenguard/region/region.h"

#include <filesystem>
#include <memory>
#include <vector>

struct sqlite3;

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
    struct DatabaseCloser {
        void operator()(sqlite3 *database) const noexcept;
    };

    std::filesystem::path path_;
    std::unique_ptr<sqlite3, DatabaseCloser> database_;
};

}  // namespace dimenguard
