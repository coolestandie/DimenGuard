#include "dimenguard/storage/schema.h"

#include "dimenguard/storage/snapshot.h"
#include "dimenguard/storage/sqlite.h"

#include <stdexcept>
#include <string>

namespace dimenguard::storage {
namespace {

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

void createGroupSchema(const sqlite::Connection &connection)
{
    connection.execute(R"sql(
        CREATE TABLE flag_groups (
            region_id INTEGER NOT NULL REFERENCES regions(id) ON DELETE CASCADE,
            name TEXT NOT NULL,
            group_name TEXT NOT NULL,
            PRIMARY KEY (region_id, name)
        );
    )sql");
}

}

void initializeSchema(const sqlite::Connection &connection)
{
    sqlite::Transaction transaction(connection, sqlite::TransactionMode::Write);
    const auto version = readVersion(connection);
    if (version == 0) {
        {
            sqlite::Statement query(connection,
                                    "SELECT name FROM sqlite_master WHERE name NOT LIKE 'sqlite_%' LIMIT 1");
            if (query.next()) {
                throw std::runtime_error("An unversioned database already contains objects; use an empty database "
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
                kind TEXT NOT NULL DEFAULT 'cuboid',
                parent TEXT,
                passthrough TEXT NOT NULL DEFAULT 'inherit',
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
                type TEXT NOT NULL,
                value TEXT NOT NULL,
                PRIMARY KEY (region_id, name)
            );
        )sql");
        createGroupSchema(connection);
        connection.execute("PRAGMA user_version = 3");
    }
    else if (version == 1 || version == 2) {
        static_cast<void>(readSnapshot(connection, version));
        const auto backup_path = connection.backupBeforeMigration(version);
        try {
            if (version == 1) {
                connection.execute(R"sql(
                    ALTER TABLE regions ADD COLUMN kind TEXT NOT NULL DEFAULT 'cuboid';
                    ALTER TABLE regions ADD COLUMN parent TEXT;
                    ALTER TABLE regions ADD COLUMN passthrough TEXT NOT NULL DEFAULT 'inherit';
                )sql");
                createGroupSchema(connection);
            }
            connection.execute(R"sql(
                ALTER TABLE flags ADD COLUMN type TEXT NOT NULL DEFAULT 'state';
                ALTER TABLE flags RENAME COLUMN state TO value;
            )sql");
            connection.execute("PRAGMA user_version = 3");
            static_cast<void>(readSnapshot(connection, schema_version));
            transaction.commit();
            return;
        }
        catch (const std::exception &error) {
            const auto encoded_path = backup_path.u8string();
            throw std::runtime_error("Schema migration failed; the original backup is at '" +
                                     std::string(encoded_path.begin(), encoded_path.end()) + "': " + error.what());
        }
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
