#include "dimenguard/storage/schema.h"

#include "dimenguard/storage/sqlite.h"

#include <stdexcept>
#include <string>

namespace dimenguard::storage {
namespace {

constexpr int schema_version = 1;

int readVersion(const sqlite::Connection &connection)
{
    sqlite::Statement query(connection, "PRAGMA user_version");
    if (!query.next()) {
        throw std::runtime_error("Could not read the database schema version.");
    }
    return query.integer32(0);
}

void checkVersion(int version)
{
    if (version != schema_version) {
        throw std::runtime_error("Unsupported database schema version " + std::to_string(version) +
                                 "; this plugin supports version " + std::to_string(schema_version) + ".");
    }
}

}

void initializeSchema(const sqlite::Connection &connection)
{
    sqlite::Transaction transaction(connection, sqlite::TransactionMode::Write);
    const auto version = readVersion(connection);
    if (version == 0) {
        {
            sqlite::Statement query(connection, "SELECT name FROM sqlite_master WHERE type = 'table' "
                                                "AND name NOT LIKE 'sqlite_%' LIMIT 1");
            if (query.next()) {
                throw std::runtime_error("An unversioned database already contains tables; use an empty database "
                                         "or restore a supported DimenGuard backup.");
            }
        }
        connection.execute(R"sql(
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
    else {
        checkVersion(version);
    }
    transaction.commit();
}

void requireSchemaVersion(const sqlite::Connection &connection)
{
    checkVersion(readVersion(connection));
}

}
