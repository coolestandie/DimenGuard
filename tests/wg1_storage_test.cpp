#include "dimenguard/storage/schema.h"
#include "dimenguard/storage/sqlite.h"
#include "dimenguard/storage/sqlite_store.h"
#include "support/database_fixture.h"
#include "support/region_assertions.h"

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace dimenguard {
namespace {

constexpr auto legacy_schema = R"sql(
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
)sql";

constexpr auto legacy_data = R"sql(
    INSERT INTO regions VALUES (7, 'world', 'Overworld', 'spawn', -16, -64, -32, 31, 319, 15, -5, 'owner');
    INSERT INTO members VALUES (7, 'trusted');
    INSERT INTO flags VALUES (7, 'build', 'deny');
    INSERT INTO flags VALUES (7, 'container-access', 'inherit');
)sql";

std::int64_t scalar(const std::filesystem::path &path, const char *sql)
{
    sqlite::Connection connection(path);
    sqlite::Statement query(connection, sql);
    if (!query.next()) {
        throw std::runtime_error("The test query returned no value.");
    }
    return query.integer(0);
}

Region legacyRegion()
{
    Region region;
    region.key = {{"world", "Overworld"}, "spawn"};
    region.bounds = {{-16, -64, -32}, {31, 319, 15}};
    region.priority = -5;
    region.owner = "owner";
    region.members = {"trusted"};
    region.flags = {{Flag::Build, FlagState::Deny}, {Flag::ContainerAccess, FlagState::Inherit}};
    return region;
}

std::vector<Region> hierarchy()
{
    auto global = legacyRegion();
    global.key.name = "__global__";
    global.kind = RegionKind::Global;
    global.priority = std::numeric_limits<int>::min();
    global.passthrough = FlagState::Deny;
    auto parent = legacyRegion();
    parent.key.name = "template";
    parent.kind = RegionKind::Template;
    parent.flags[Flag::Pvp] = FlagState::Deny;
    parent.flag_groups[Flag::Pvp] = RegionGroup::NonMembers;
    auto child = legacyRegion();
    child.parent = parent.key.name;
    child.priority = 10;
    child.passthrough = FlagState::Allow;
    child.flag_groups[Flag::Build] = RegionGroup::Owners;
    child.flag_groups[Flag::Pvp] = RegionGroup::NonOwners;
    return {global, parent, child};
}

class Wg1StorageTest : public test::DatabaseFixture {
protected:
    void createLegacy() const
    {
        executeRaw(legacy_schema);
        executeRaw(legacy_data);
    }

    [[nodiscard]] std::vector<std::filesystem::path> backups() const
    {
        std::vector<std::filesystem::path> result;
        for (const auto &entry : std::filesystem::directory_iterator(directory_)) {
            if (entry.is_directory() && entry.path().filename().string().find(".v1-backup-") != std::string::npos) {
                result.push_back(entry.path() / path_.filename());
            }
        }
        return result;
    }

    void expectLegacy(const std::filesystem::path &path) const
    {
        EXPECT_EQ(scalar(path, "PRAGMA user_version"), 1);
        EXPECT_EQ(scalar(path, "SELECT COUNT(*) FROM pragma_table_info('regions')"), 12);
        EXPECT_EQ(scalar(path, "SELECT COUNT(*) FROM regions WHERE id = 7 AND name = 'spawn' AND priority = -5"), 1);
        EXPECT_EQ(scalar(path, "SELECT COUNT(*) FROM members WHERE region_id = 7 AND identity = 'trusted'"), 1);
        EXPECT_EQ(scalar(path, "SELECT COUNT(*) FROM flags WHERE region_id = 7"), 2);
    }
};

TEST_F(Wg1StorageTest, LegacyMigrationPreservesDataAndKeepsAStandaloneOriginalBackup)
{
    createLegacy();
    SqliteStore store(path_);
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    test::expectRegionEqual(loaded.front(), legacyRegion());
    EXPECT_EQ(scalar(path_, "PRAGMA user_version"), storage::schema_version);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM flags WHERE type = 'state'"), 2);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM flags WHERE value = 'inherit'"), 1);
    const auto files = backups();
    ASSERT_EQ(files.size(), 1);
    for (const auto &entry : std::filesystem::directory_iterator(directory_)) {
        EXPECT_EQ(entry.path().filename().string().find(".v2-backup-"), std::string::npos);
    }
    expectLegacy(files.front());
    store.save({});
    expectLegacy(files.front());
}

TEST_F(Wg1StorageTest, LegacyReservedNameRemainsAnOrdinaryCuboid)
{
    createLegacy();
    executeRaw("UPDATE regions SET name = '__global__'");
    SqliteStore store(path_);
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    auto expected = legacyRegion();
    expected.key.name = "__global__";
    test::expectRegionEqual(loaded.front(), expected);
}

TEST_F(Wg1StorageTest, MigrationBackupIncludesCommittedWalPages)
{
    test::RawDatabase writer(path_);
    writer.execute("PRAGMA journal_mode = WAL; PRAGMA wal_autocheckpoint = 0");
    writer.execute(legacy_schema);
    writer.execute(legacy_data);
    ASSERT_GT(std::filesystem::file_size(path_.string() + "-wal"), 0);
    SqliteStore store(path_);
    ASSERT_EQ(store.load().size(), 1);
    const auto files = backups();
    ASSERT_EQ(files.size(), 1);
    expectLegacy(files.front());
}

TEST_F(Wg1StorageTest, InvalidLegacyDataDoesNotPartiallyMigrate)
{
    createLegacy();
    executeRaw("UPDATE flags SET state = 'future-state' WHERE name = 'build'");
    EXPECT_THROW(SqliteStore{path_}, std::runtime_error);
    expectLegacy(path_);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM flags WHERE state = 'future-state'"), 1);
    executeRaw("UPDATE flags SET state = 'deny' WHERE name = 'build'");
    SqliteStore recovered(path_);
    ASSERT_EQ(recovered.load().size(), 1);
}

TEST_F(Wg1StorageTest, FailedMigrationRollsBackAllDdlAndDoesNotOverwritePreviousBackups)
{
    createLegacy();
    executeRaw("CREATE TABLE flag_groups (sentinel TEXT); INSERT INTO flag_groups VALUES ('keep')");
    EXPECT_THROW(SqliteStore{path_}, std::runtime_error);
    expectLegacy(path_);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM flag_groups WHERE sentinel = 'keep'"), 1);
    const auto first_files = backups();
    ASSERT_EQ(first_files.size(), 1);
    expectLegacy(first_files.front());
    executeRaw("DROP TABLE flag_groups");
    SqliteStore recovered(path_);
    ASSERT_EQ(recovered.load().size(), 1);
    EXPECT_EQ(backups().size(), 2);
    expectLegacy(first_files.front());
    EXPECT_EQ(scalar(first_files.front(), "SELECT COUNT(*) FROM flag_groups WHERE sentinel = 'keep'"), 1);
}

TEST_F(Wg1StorageTest, TypedFlagDdlFailureAlsoRollsBackNewRegionColumnsAndGroups)
{
    createLegacy();
    executeRaw("ALTER TABLE flags ADD COLUMN type TEXT NOT NULL DEFAULT 'keep'");
    EXPECT_THROW(SqliteStore{path_}, std::runtime_error);
    expectLegacy(path_);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM sqlite_master WHERE name = 'flag_groups'"), 0);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM pragma_table_info('flags') WHERE name = 'value'"), 0);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM flags WHERE type = 'keep'"), 2);
    const auto files = backups();
    ASSERT_EQ(files.size(), 1);
    expectLegacy(files.front());
}

TEST_F(Wg1StorageTest, NewFlagNamesCannotLegitimizeCorruptVersionOneData)
{
    createLegacy();
    for (const auto *name : {"tnt", "creeper-explosion", "other-explosion", "deny-spawn", "entry-deny-message",
                             "exit-deny-message", "nonplayer-protection-domains"}) {
        SCOPED_TRACE(name);
        executeRaw("UPDATE flags SET name = '" + std::string(name) + "' WHERE name = 'build'");
        EXPECT_THROW(SqliteStore{path_}, std::runtime_error);
        expectLegacy(path_);
        EXPECT_TRUE(backups().empty());
        executeRaw("UPDATE flags SET name = 'build' WHERE name = '" + std::string(name) + "'");
    }
}

TEST_F(Wg1StorageTest, DirectTypedMigrationRetainsEveryLegacyStateFlag)
{
    createLegacy();
    executeRaw("DELETE FROM flags");
    auto expected = legacyRegion();
    expected.flags.clear();
    for (const auto *name : {"build",
                             "interact",
                             "container-access",
                             "pvp",
                             "explosions",
                             "fluid-flow",
                             "block-form",
                             "leaf-decay",
                             "actor-griefing",
                             "mob-spawning",
                             "mob-damage",
                             "entry",
                             "exit",
                             "block-break",
                             "block-place",
                             "use",
                             "use-anvil",
                             "sleep",
                             "item-drop",
                             "item-pickup",
                             "send-chat",
                             "water-flow",
                             "lava-flow",
                             "fall-damage",
                             "firework-damage",
                             "invincible"}) {
        const auto flag = parseFlag(name);
        ASSERT_TRUE(flag);
        expected.flags[*flag] = FlagState::Deny;
        executeRaw("INSERT INTO flags VALUES (7, '" + std::string(name) + "', 'deny')");
    }
    SqliteStore store(path_);
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    test::expectRegionEqual(loaded.front(), expected);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM flags WHERE type = 'state' AND value = 'deny'"), 26);
    const auto files = backups();
    ASSERT_EQ(files.size(), 1);
    EXPECT_EQ(scalar(files.front(), "PRAGMA user_version"), 1);
    EXPECT_EQ(scalar(files.front(), "SELECT COUNT(*) FROM flags WHERE state = 'deny'"), 26);
}

TEST_F(Wg1StorageTest, FailedMigrationCommitRestoresTheOriginalSchema)
{
    createLegacy();
    test::RawDatabase reader(path_);
    reader.execute("BEGIN; SELECT * FROM regions");
    {
        sqlite::Connection connection(path_, 0);
        EXPECT_THROW(storage::initializeSchema(connection), std::runtime_error);
    }
    reader.execute("ROLLBACK");
    expectLegacy(path_);
    const auto files = backups();
    ASSERT_EQ(files.size(), 1);
    expectLegacy(files.front());
}

TEST_F(Wg1StorageTest, UnavailableFileBackupAbortsBeforeChangingLegacySchema)
{
    sqlite::Connection connection(":memory:");
    connection.execute(legacy_schema);
    connection.execute(legacy_data);
    EXPECT_THROW(storage::initializeSchema(connection), std::runtime_error);
    sqlite::Statement version(connection, "PRAGMA user_version");
    ASSERT_TRUE(version.next());
    EXPECT_EQ(version.integer(0), 1);
    sqlite::Statement columns(connection, "SELECT COUNT(*) FROM pragma_table_info('regions')");
    ASSERT_TRUE(columns.next());
    EXPECT_EQ(columns.integer(0), 12);
    sqlite::Statement regions(connection, "SELECT COUNT(*) FROM regions WHERE id = 7 AND name = 'spawn'");
    ASSERT_TRUE(regions.next());
    EXPECT_EQ(regions.integer(0), 1);
}

TEST_F(Wg1StorageTest, FutureOrMissingVersionDoesNotMigrateOrEraseLegacyData)
{
    createLegacy();
    for (const auto version : {0, -1, 99}) {
        executeRaw("PRAGMA user_version = " + std::to_string(version));
        EXPECT_THROW(SqliteStore{path_}, std::runtime_error);
        EXPECT_EQ(scalar(path_, "PRAGMA user_version"), version);
        EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM regions"), 1);
    }
    EXPECT_TRUE(backups().empty());
}

TEST_F(Wg1StorageTest, AnUnversionedViewIsNotRepurposedAsANewDatabase)
{
    executeRaw("CREATE VIEW unrelated AS SELECT 'keep' AS value");
    EXPECT_THROW(SqliteStore{path_}, std::runtime_error);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM unrelated WHERE value = 'keep'"), 1);
    EXPECT_EQ(scalar(path_, "PRAGMA user_version"), 0);
}

TEST_F(Wg1StorageTest, NewSnapshotRoundTripsKindsParentsPassthroughAndGroupOnlyOverrides)
{
    auto expected = hierarchy();
    {
        SqliteStore store(path_);
        store.save(expected);
    }
    SqliteStore reopened(path_);
    const auto loaded = reopened.load();
    std::ranges::sort(expected, {}, &Region::key);
    ASSERT_EQ(loaded.size(), expected.size());
    for (std::size_t index = 0; index < loaded.size(); ++index) {
        test::expectRegionEqual(loaded[index], expected[index]);
    }
    reopened.save({});
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM flag_groups"), 0);
    EXPECT_TRUE(backups().empty());
}

TEST_F(Wg1StorageTest, InvalidGraphSavePreservesTheCommittedSnapshot)
{
    SqliteStore store(path_);
    const auto original = hierarchy();
    store.save(original);
    auto invalid = original;
    invalid.back().parent = "missing";
    EXPECT_THROW(store.save(invalid), std::runtime_error);
    invalid = original;
    invalid[1].parent = "spawn";
    invalid[1].priority = invalid.back().priority;
    EXPECT_THROW(store.save(invalid), std::runtime_error);
    invalid = original;
    invalid[1].key.dimension.dimension = "Nether";
    EXPECT_THROW(store.save(invalid), std::runtime_error);
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), original.size());
    EXPECT_EQ(loaded.back().kind, RegionKind::Template);
    EXPECT_FALSE(loaded.back().parent);
}

TEST_F(Wg1StorageTest, StoredGraphAndEnumsAreValidatedOnEveryLoad)
{
    SqliteStore store(path_);
    const auto original = hierarchy();
    const std::vector<std::string> corruptions = {
        "UPDATE regions SET kind = 'future-kind' WHERE name = 'spawn'",
        "UPDATE regions SET kind = x'00' WHERE name = 'spawn'",
        "UPDATE regions SET parent = '' WHERE name = 'spawn'",
        "UPDATE regions SET parent = x'00' WHERE name = 'spawn'",
        "UPDATE regions SET parent = 'missing' WHERE name = 'spawn'",
        "UPDATE regions SET parent = 'spawn' WHERE name = 'spawn'",
        "UPDATE regions SET parent = 'spawn', priority = 10 WHERE name = 'template'",
        "UPDATE regions SET priority = -10 WHERE name = 'spawn'",
        "UPDATE regions SET priority = 0 WHERE kind = 'global'",
        "UPDATE regions SET passthrough = 'future-state' WHERE name = 'spawn'",
        "UPDATE flag_groups SET group_name = 'future-group' WHERE name = 'pvp'",
        "UPDATE flag_groups SET group_name = x'00' WHERE name = 'pvp'",
        "UPDATE flag_groups SET name = 'future-flag' WHERE name = 'pvp'",
        "UPDATE flag_groups SET region_id = 9999 WHERE name = 'pvp' "
        "AND region_id = (SELECT id FROM regions WHERE name = 'spawn')",
        "INSERT INTO flag_groups SELECT id, 'explosions', 'members' FROM regions WHERE name = 'spawn'"};
    for (const auto &corruption : corruptions) {
        SCOPED_TRACE(corruption);
        store.save(original);
        executeRaw(corruption);
        EXPECT_THROW(static_cast<void>(store.load()), std::runtime_error);
        executeRaw("DELETE FROM flag_groups");
    }
}

TEST_F(Wg1StorageTest, FailedGroupWriteRollsBackRegionMemberAndFlagReplacement)
{
    SqliteStore store(path_);
    const auto original = legacyRegion();
    store.save({original});
    executeRaw("CREATE TRIGGER reject_groups BEFORE INSERT ON flag_groups "
               "BEGIN SELECT RAISE(ABORT, 'injected write failure'); END");
    EXPECT_THROW(store.save(hierarchy()), std::runtime_error);
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    test::expectRegionEqual(loaded.front(), original);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM flag_groups"), 0);
}

}
}
