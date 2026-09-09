#include "dimenguard/storage/sqlite_store.h"

#include "dimenguard/storage/limits.h"
#include "dimenguard/storage/schema.h"
#include "dimenguard/storage/sqlite.h"

#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace dimenguard {
namespace {

struct LoadedSnapshot {
    std::vector<Region> regions;
    std::map<std::int64_t, std::size_t> row_ids;

    Region &find(std::int64_t id)
    {
        const auto found = row_ids.find(id);
        if (found == row_ids.end()) {
            throw std::runtime_error("The database contains a member or flag referencing a missing region.");
        }
        return regions.at(found->second);
    }
};

std::size_t countRegions(const sqlite::Connection &connection)
{
    sqlite::Statement query(connection, "SELECT COUNT(*) FROM regions");
    if (!query.next()) {
        throw std::runtime_error("Could not count stored regions.");
    }
    const auto count = query.integer(0);
    if (count < 0) {
        throw std::runtime_error("The stored region count is invalid.");
    }
    if (static_cast<std::uint64_t>(count) > storage::max_regions) {
        throw storage::SnapshotLimitError{};
    }
    return static_cast<std::size_t>(count);
}

LoadedSnapshot readRegions(const sqlite::Connection &connection)
{
    LoadedSnapshot snapshot;
    snapshot.regions.reserve(countRegions(connection));
    sqlite::Statement query(connection, "SELECT id, level, dimension, name, min_x, min_y, min_z, max_x, max_y, max_z, "
                                        "priority, owner FROM regions ORDER BY level, dimension, name");
    while (query.next()) {
        Region region;
        region.key = {{query.text(1), query.text(2)}, query.text(3)};
        region.bounds = {{query.integer32(4), query.integer32(5), query.integer32(6)},
                         {query.integer32(7), query.integer32(8), query.integer32(9)}};
        region.priority = query.integer32(10);
        region.owner = query.text(11);
        if (!snapshot.row_ids.emplace(query.integer(0), snapshot.regions.size()).second) {
            throw std::runtime_error("The database contains duplicate region identifiers.");
        }
        snapshot.regions.push_back(std::move(region));
    }
    return snapshot;
}

void readMembers(const sqlite::Connection &connection, LoadedSnapshot &snapshot)
{
    sqlite::Statement query(connection, "SELECT region_id, identity FROM members");
    while (query.next()) {
        if (!snapshot.find(query.integer(0)).members.insert(query.text(1)).second) {
            throw std::runtime_error("The database contains a duplicate region member.");
        }
    }
}

void readFlags(const sqlite::Connection &connection, LoadedSnapshot &snapshot)
{
    sqlite::Statement query(connection, "SELECT region_id, name, state FROM flags");
    while (query.next()) {
        const auto name = query.text(1);
        const auto state_name = query.text(2);
        const auto flag = parseFlag(name);
        const auto state = parseState(state_name);
        if (!flag || !state) {
            throw std::runtime_error("Unsupported stored flag '" + name + "' or state '" + state_name + "'.");
        }
        if (!snapshot.find(query.integer(0)).flags.emplace(*flag, *state).second) {
            throw std::runtime_error("The database contains a duplicate region flag.");
        }
    }
}

void writeRegions(const sqlite::Connection &connection, const std::vector<Region> &regions)
{
    sqlite::Statement insert_region(connection,
                                    "INSERT INTO regions (level, dimension, name, min_x, min_y, min_z, max_x, "
                                    "max_y, max_z, priority, owner) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    sqlite::Statement insert_member(connection, "INSERT INTO members (region_id, identity) VALUES (?, ?)");
    sqlite::Statement insert_flag(connection, "INSERT INTO flags (region_id, name, state) VALUES (?, ?, ?)");
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
        const auto id = connection.lastInsertId();
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
}

}

SqliteStore::SqliteStore(const std::filesystem::path &path) : database_(std::make_unique<sqlite::Connection>(path))
{
    try {
        storage::initializeSchema(*database_);
    }
    catch (const std::exception &error) {
        throw std::runtime_error("Cannot initialize region database '" + database_->getPath() + "': " + error.what());
    }
}

SqliteStore::~SqliteStore() = default;

std::vector<Region> SqliteStore::load() const
{
    try {
        sqlite::Transaction transaction(*database_, sqlite::TransactionMode::Read);
        storage::requireSchemaVersion(*database_);
        auto snapshot = readRegions(*database_);
        readMembers(*database_, snapshot);
        readFlags(*database_, snapshot);
        std::set<RegionKey> keys;
        for (const auto &region : snapshot.regions) {
            validateRegion(region);
            if (!keys.insert(region.key).second) {
                throw std::runtime_error("The database contains duplicate region names in the same dimension.");
            }
        }
        transaction.commit();
        return std::move(snapshot.regions);
    }
    catch (const storage::SnapshotLimitError &) {
        throw;
    }
    catch (const std::exception &error) {
        throw std::runtime_error("Cannot load regions from '" + database_->getPath() + "': " + error.what());
    }
}

void SqliteStore::save(const std::vector<Region> &regions)
{
    if (regions.size() > storage::max_regions) {
        throw storage::SnapshotLimitError{};
    }
    try {
        for (const auto &region : regions) {
            validateRegion(region);
        }
        sqlite::Transaction transaction(*database_, sqlite::TransactionMode::Write);
        storage::requireSchemaVersion(*database_);
        database_->execute("DELETE FROM regions");
        writeRegions(*database_, regions);
        transaction.commit();
    }
    catch (const std::exception &error) {
        throw std::runtime_error("Cannot save regions to '" + database_->getPath() + "': " + error.what());
    }
}

}
