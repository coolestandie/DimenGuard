#include "dimenguard/storage/sqlite_store.h"

#include "dimenguard/storage/limits.h"
#include "dimenguard/storage/schema.h"
#include "dimenguard/storage/snapshot.h"
#include "dimenguard/storage/sqlite.h"

#include <stdexcept>
#include <string>

namespace dimenguard {

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
        auto regions = storage::readSnapshot(*database_, storage::schema_version);
        transaction.commit();
        return regions;
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
        validateRegions(regions);
        sqlite::Transaction transaction(*database_, sqlite::TransactionMode::Write);
        storage::requireSchemaVersion(*database_);
        database_->execute("DELETE FROM regions");
        storage::writeSnapshot(*database_, regions);
        transaction.commit();
    }
    catch (const std::exception &error) {
        throw std::runtime_error("Cannot save regions to '" + database_->getPath() + "': " + error.what());
    }
}

}
