#include "dimenguard/storage/sqlite_store.h"

#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace dimenguard {
namespace {

constexpr int schema_version = 1;

[[nodiscard]] std::string pathText(const std::filesystem::path &path)
{
    const auto text = path.u8string();
    return {text.begin(), text.end()};
}

void execute(sqlite3 *database, const char *sql)
{
    if (sqlite3_exec(database, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(database));
    }
}

class Transaction {
public:
    explicit Transaction(sqlite3 *database, bool write) : database_(database)
    {
        execute(database_, write ? "BEGIN IMMEDIATE" : "BEGIN");
    }

    ~Transaction()
    {
        if (!committed_) {
            sqlite3_exec(database_, "ROLLBACK", nullptr, nullptr, nullptr);
        }
    }

    Transaction(const Transaction &) = delete;
    Transaction &operator=(const Transaction &) = delete;

    void commit()
    {
        execute(database_, "COMMIT");
        committed_ = true;
    }

private:
    sqlite3 *database_;
    bool committed_{false};
};

class Statement {
public:
    Statement(sqlite3 *database, const char *sql) : database_(database)
    {
        if (sqlite3_prepare_v2(database, sql, -1, &statement_, nullptr) != SQLITE_OK) {
            const std::string message = sqlite3_errmsg(database);
            sqlite3_finalize(statement_);
            throw std::runtime_error(message);
        }
    }

    ~Statement() { sqlite3_finalize(statement_); }

    Statement(const Statement &) = delete;
    Statement &operator=(const Statement &) = delete;

    void bind(int index, std::string_view value)
    {
        if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("A region text field exceeds SQLite's supported length.");
        }
        check(sqlite3_bind_text(statement_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT));
    }

    void bind(int index, std::int64_t value) { check(sqlite3_bind_int64(statement_, index, value)); }

    [[nodiscard]] bool next()
    {
        const auto result = sqlite3_step(statement_);
        if (result == SQLITE_ROW) {
            return true;
        }
        if (result == SQLITE_DONE) {
            return false;
        }
        throw std::runtime_error(sqlite3_errmsg(database_));
    }

    void run()
    {
        if (next()) {
            throw std::runtime_error("An unexpected row was returned while saving regions.");
        }
        check(sqlite3_reset(statement_));
        check(sqlite3_clear_bindings(statement_));
    }

    [[nodiscard]] std::int64_t integer(int column) const
    {
        requireType(column, SQLITE_INTEGER);
        return sqlite3_column_int64(statement_, column);
    }

    [[nodiscard]] int coordinate(int column) const
    {
        const auto value = integer(column);
        if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
            throw std::runtime_error("A stored coordinate or priority is outside the supported integer range.");
        }
        return static_cast<int>(value);
    }

    [[nodiscard]] std::string text(int column) const
    {
        requireType(column, SQLITE_TEXT);
        const auto *value = sqlite3_column_text(statement_, column);
        if (value == nullptr) {
            throw std::runtime_error("Could not read a stored text field: SQLite ran out of memory.");
        }
        return {reinterpret_cast<const char *>(value),
                static_cast<std::size_t>(sqlite3_column_bytes(statement_, column))};
    }

private:
    void check(int result) const
    {
        if (result != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(database_));
        }
    }

    void requireType(int column, int expected) const
    {
        if (sqlite3_column_type(statement_, column) != expected) {
            throw std::runtime_error("A stored region field has an invalid type; repair the database before loading.");
        }
    }

    sqlite3 *database_;
    sqlite3_stmt *statement_{nullptr};
};

void migrate(sqlite3 *database)
{
    Transaction transaction(database, true);
    int version = 0;
    {
        Statement query(database, "PRAGMA user_version");
        if (!query.next()) {
            throw std::runtime_error("Could not read the database schema version.");
        }
        version = query.coordinate(0);
    }
    if (version != 0 && version != schema_version) {
        throw std::runtime_error("Unsupported database schema version " + std::to_string(version) +
                                 "; this plugin supports version " + std::to_string(schema_version) + ".");
    }
    if (version == 0) {
        {
            Statement query(database, "SELECT name FROM sqlite_master WHERE type = 'table' "
                                      "AND name NOT LIKE 'sqlite_%' LIMIT 1");
            if (query.next()) {
                throw std::runtime_error("An unversioned database already contains tables; use an empty database "
                                         "or restore a supported DimenGuard backup.");
            }
        }
        execute(database, R"sql(
            CREATE TABLE regions (
                id INTEGER PRIMARY KEY,
                level TEXT NOT NULL,
                dimension TEXT NOT NULL,
                name TEXT NOT NULL,
                min_x INTEGER NOT NULL, min_y INTEGER NOT NULL, min_z INTEGER NOT NULL,
                max_x INTEGER NOT NULL, max_y INTEGER NOT NULL, max_z INTEGER NOT NULL,
                priority INTEGER NOT NULL,
                owner TEXT NOT NULL,
                UNIQUE (level, dimension, name),
                CHECK (min_x <= max_x AND min_y <= max_y AND min_z <= max_z)
            );
            CREATE TABLE members (
                region_id INTEGER NOT NULL REFERENCES regions(id) ON DELETE CASCADE,
                identity TEXT NOT NULL,
                PRIMARY KEY (region_id, identity)
            );
            CREATE TABLE flags (
                region_id INTEGER NOT NULL REFERENCES regions(id) ON DELETE CASCADE,
                name TEXT NOT NULL,
                state TEXT NOT NULL,
                PRIMARY KEY (region_id, name)
            );
            PRAGMA user_version = 1;
        )sql");
    }
    transaction.commit();
}

}  // namespace

void SqliteStore::DatabaseCloser::operator()(sqlite3 *database) const noexcept
{
    sqlite3_close_v2(database);
}

SqliteStore::SqliteStore(const std::filesystem::path &path) : path_(path)
{
    try {
        if (path_.empty()) {
            throw std::invalid_argument("The region database path must not be empty.");
        }
        if (!path_.parent_path().empty()) {
            std::filesystem::create_directories(path_.parent_path());
        }
        sqlite3 *database = nullptr;
        const auto result =
            sqlite3_open_v2(pathText(path_).c_str(), &database,
                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
        database_.reset(database);
        if (result != SQLITE_OK) {
            throw std::runtime_error(database ? sqlite3_errmsg(database) : "SQLite could not allocate a connection.");
        }
        if (sqlite3_busy_timeout(database, 2000) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(database));
        }
        execute(database, "PRAGMA foreign_keys = ON");
        migrate(database);
    }
    catch (const std::exception &error) {
        throw std::runtime_error("Cannot open region database '" + pathText(path_) + "': " + error.what());
    }
}

SqliteStore::~SqliteStore() = default;

std::vector<Region> SqliteStore::load() const
{
    try {
        auto *database = database_.get();
        Transaction transaction(database, false);
        std::vector<Region> regions;
        std::map<std::int64_t, std::size_t> indices;
        {
            Statement query(database, "SELECT id, level, dimension, name, min_x, min_y, min_z, max_x, max_y, max_z, "
                                      "priority, owner FROM regions ORDER BY level, dimension, name");
            while (query.next()) {
                Region region;
                region.key = {{query.text(1), query.text(2)}, query.text(3)};
                region.bounds = {{query.coordinate(4), query.coordinate(5), query.coordinate(6)},
                                 {query.coordinate(7), query.coordinate(8), query.coordinate(9)}};
                region.priority = query.coordinate(10);
                region.owner = query.text(11);
                if (!indices.emplace(query.integer(0), regions.size()).second) {
                    throw std::runtime_error("The database contains duplicate region identifiers.");
                }
                regions.push_back(std::move(region));
            }
        }
        const auto find_region = [&](std::int64_t id) -> Region & {
            const auto found = indices.find(id);
            if (found == indices.end()) {
                throw std::runtime_error("The database contains a member or flag referencing a missing region.");
            }
            return regions.at(found->second);
        };
        {
            Statement query(database, "SELECT region_id, identity FROM members");
            while (query.next()) {
                if (!find_region(query.integer(0)).members.insert(query.text(1)).second) {
                    throw std::runtime_error("The database contains a duplicate region member.");
                }
            }
        }
        {
            Statement query(database, "SELECT region_id, name, state FROM flags");
            while (query.next()) {
                const auto name = query.text(1);
                const auto state_name = query.text(2);
                const auto flag = parseFlag(name);
                const auto state = parseState(state_name);
                if (!flag || !state) {
                    throw std::runtime_error("Unsupported stored flag '" + name + "' or state '" + state_name + "'.");
                }
                if (!find_region(query.integer(0)).flags.emplace(*flag, *state).second) {
                    throw std::runtime_error("The database contains a duplicate region flag.");
                }
            }
        }
        std::set<RegionKey> keys;
        for (const auto &region : regions) {
            validateRegion(region);
            if (!keys.insert(region.key).second) {
                throw std::runtime_error("The database contains duplicate region names in the same dimension.");
            }
        }
        transaction.commit();
        return regions;
    }
    catch (const std::exception &error) {
        throw std::runtime_error("Cannot load regions from '" + pathText(path_) + "': " + error.what());
    }
}

void SqliteStore::save(const std::vector<Region> &regions)
{
    try {
        for (const auto &region : regions) {
            validateRegion(region);
        }
        auto *database = database_.get();
        Transaction transaction(database, true);
        execute(database, "DELETE FROM regions");
        Statement insert_region(database, "INSERT INTO regions (level, dimension, name, min_x, min_y, min_z, max_x, "
                                          "max_y, max_z, priority, owner) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        Statement insert_member(database, "INSERT INTO members (region_id, identity) VALUES (?, ?)");
        Statement insert_flag(database, "INSERT INTO flags (region_id, name, state) VALUES (?, ?, ?)");
        for (const auto &region : regions) {
            insert_region.bind(1, region.key.dimension.level);
            insert_region.bind(2, region.key.dimension.dimension);
            insert_region.bind(3, region.key.name);
            insert_region.bind(4, region.bounds.min.x);
            insert_region.bind(5, region.bounds.min.y);
            insert_region.bind(6, region.bounds.min.z);
            insert_region.bind(7, region.bounds.max.x);
            insert_region.bind(8, region.bounds.max.y);
            insert_region.bind(9, region.bounds.max.z);
            insert_region.bind(10, region.priority);
            insert_region.bind(11, region.owner);
            insert_region.run();
            const auto id = sqlite3_last_insert_rowid(database);
            for (const auto &member : region.members) {
                insert_member.bind(1, id);
                insert_member.bind(2, member);
                insert_member.run();
            }
            for (const auto &[flag, state] : region.flags) {
                insert_flag.bind(1, id);
                insert_flag.bind(2, flagName(flag));
                insert_flag.bind(3, stateName(state));
                insert_flag.run();
            }
        }
        transaction.commit();
    }
    catch (const std::exception &error) {
        throw std::runtime_error("Cannot save regions to '" + pathText(path_) + "': " + error.what());
    }
}

}  // namespace dimenguard
