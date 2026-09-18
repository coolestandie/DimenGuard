#include "dimenguard/storage/schema.h"
#include "dimenguard/storage/sqlite.h"
#include "dimenguard/storage/sqlite_store.h"
#include "support/database_fixture.h"
#include "support/region_assertions.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace dimenguard {
namespace {

constexpr auto schema_two = R"sql(
    CREATE TABLE regions (
        id INTEGER PRIMARY KEY,
        level TEXT NOT NULL, dimension TEXT NOT NULL, name TEXT NOT NULL,
        min_x INTEGER NOT NULL, min_y INTEGER NOT NULL, min_z INTEGER NOT NULL,
        max_x INTEGER NOT NULL, max_y INTEGER NOT NULL, max_z INTEGER NOT NULL,
        priority INTEGER NOT NULL, owner TEXT NOT NULL,
        kind TEXT NOT NULL DEFAULT 'cuboid', parent TEXT, passthrough TEXT NOT NULL DEFAULT 'inherit',
        UNIQUE (level, dimension, name),
        CHECK (min_x <= max_x AND min_y <= max_y AND min_z <= max_z)
    );
    CREATE TABLE members (
        region_id INTEGER NOT NULL REFERENCES regions(id) ON DELETE CASCADE,
        identity TEXT NOT NULL, PRIMARY KEY (region_id, identity)
    );
    CREATE TABLE flags (
        region_id INTEGER NOT NULL REFERENCES regions(id) ON DELETE CASCADE,
        name TEXT NOT NULL, state TEXT NOT NULL, PRIMARY KEY (region_id, name)
    );
    CREATE TABLE flag_groups (
        region_id INTEGER NOT NULL REFERENCES regions(id) ON DELETE CASCADE,
        name TEXT NOT NULL, group_name TEXT NOT NULL, PRIMARY KEY (region_id, name)
    );
    PRAGMA user_version = 2;
)sql";

constexpr auto schema_two_data = R"sql(
    INSERT INTO regions VALUES (4, 'world', 'Overworld', '__global__', 0, 0, 0, 0, 0, 0,
                                -2147483648, 'owner', 'global', NULL, 'inherit');
    INSERT INTO regions VALUES (7, 'world', 'Overworld', 'parent', -16, -64, -32, 31, 319, 15,
                                -5, 'owner', 'template', NULL, 'deny');
    INSERT INTO regions VALUES (9, 'world', 'Overworld', 'child', -16, -64, -32, 31, 319, 15,
                                5, 'child-owner', 'cuboid', 'parent', 'allow');
    INSERT INTO members VALUES (7, 'trusted');
    INSERT INTO flags VALUES (4, 'pvp', 'deny');
    INSERT INTO flags VALUES (7, 'build', 'deny');
    INSERT INTO flags VALUES (9, 'container-access', 'inherit');
    INSERT INTO flags VALUES (9, 'entry', 'allow');
    INSERT INTO flag_groups VALUES (7, 'build', 'nonmembers');
    INSERT INTO flag_groups VALUES (9, 'entry', 'owners');
    INSERT INTO flag_groups VALUES (9, 'exit', 'nonowners');
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

FlagValue value(Flag flag, std::string_view text)
{
    const auto parsed = parseFlagValue(flag, text);
    if (!parsed) {
        throw std::runtime_error("The test flag value is invalid.");
    }
    return *parsed;
}

Region typedRegion()
{
    Region region;
    region.key = {{"world", "Overworld"}, "spawn"};
    region.bounds = {{-16, -64, -32}, {31, 319, 15}};
    region.owner = "owner";
    region.flags = {{Flag::Build, FlagState::Deny},
                    {Flag::DenySpawn, value(Flag::DenySpawn, "minecraft:zombie,minecraft:creeper")},
                    {Flag::EntryDenyMessage, value(Flag::EntryDenyMessage, "Owners' private region")},
                    {Flag::ExitDenyMessage, value(Flag::ExitDenyMessage, "inherit")},
                    {Flag::NonPlayerProtectionDomains, value(Flag::NonPlayerProtectionDomains, "castle,farm")}};
    region.flag_groups[Flag::EntryDenyMessage] = RegionGroup::NonOwners;
    return region;
}

std::vector<Region> legacyRegions()
{
    Region global;
    global.key = {{"world", "Overworld"}, "__global__"};
    global.owner = "owner";
    global.kind = RegionKind::Global;
    global.priority = std::numeric_limits<int>::min();
    global.flags[Flag::Pvp] = FlagState::Deny;
    Region parent;
    parent.key = {global.key.dimension, "parent"};
    parent.bounds = {{-16, -64, -32}, {31, 319, 15}};
    parent.priority = -5;
    parent.owner = "owner";
    parent.members = {"trusted"};
    parent.kind = RegionKind::Template;
    parent.passthrough = FlagState::Deny;
    parent.flags[Flag::Build] = FlagState::Deny;
    parent.flag_groups[Flag::Build] = RegionGroup::NonMembers;
    Region child;
    child.key = {global.key.dimension, "child"};
    child.bounds = parent.bounds;
    child.priority = 5;
    child.owner = "child-owner";
    child.parent = "parent";
    child.passthrough = FlagState::Allow;
    child.flags = {{Flag::ContainerAccess, FlagState::Inherit}, {Flag::Entry, FlagState::Allow}};
    child.flag_groups = {{Flag::Entry, RegionGroup::Owners}, {Flag::Exit, RegionGroup::NonOwners}};
    return {global, child, parent};
}

class Wg2StorageTest : public test::DatabaseFixture {
protected:
    void createLegacy() const
    {
        executeRaw(schema_two);
        executeRaw(schema_two_data);
    }

    [[nodiscard]] std::vector<std::filesystem::path> backups() const
    {
        std::vector<std::filesystem::path> result;
        for (const auto &entry : std::filesystem::directory_iterator(directory_)) {
            if (entry.is_directory() && entry.path().filename().string().find(".v2-backup-") != std::string::npos) {
                result.push_back(entry.path() / path_.filename());
            }
        }
        return result;
    }

    void expectLegacy(const std::filesystem::path &path) const
    {
        EXPECT_EQ(scalar(path, "PRAGMA user_version"), 2);
        EXPECT_EQ(scalar(path, "SELECT COUNT(*) FROM pragma_table_info('flags') WHERE name = 'state'"), 1);
        EXPECT_EQ(scalar(path, "SELECT COUNT(*) FROM pragma_table_info('flags') WHERE name = 'value'"), 0);
        EXPECT_EQ(scalar(path, "SELECT COUNT(*) FROM regions WHERE id = 9 AND parent = 'parent'"), 1);
        EXPECT_EQ(scalar(path, "SELECT COUNT(*) FROM members WHERE region_id = 7 AND identity = 'trusted'"), 1);
        EXPECT_EQ(scalar(path, "SELECT COUNT(*) FROM flags"), 4);
        EXPECT_EQ(scalar(path, "SELECT COUNT(*) FROM flag_groups"), 3);
    }
};

TEST_F(Wg2StorageTest, MigratesVersionTwoWithoutChangingHierarchyOrStateGroups)
{
    createLegacy();
    SqliteStore store(path_);
    const auto loaded = store.load();
    const auto expected = legacyRegions();
    ASSERT_EQ(loaded.size(), expected.size());
    for (std::size_t index = 0; index < loaded.size(); ++index) {
        test::expectRegionEqual(loaded[index], expected[index]);
    }
    EXPECT_EQ(scalar(path_, "PRAGMA user_version"), storage::schema_version);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM flags WHERE type = 'state'"), 4);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM flags WHERE value = 'inherit'"), 1);
    const auto files = backups();
    ASSERT_EQ(files.size(), 1);
    expectLegacy(files.front());
    store.save({typedRegion()});
    expectLegacy(files.front());
    SqliteStore reopened(path_);
    EXPECT_EQ(reopened.load().size(), 1);
    EXPECT_EQ(backups().size(), 1);
}

TEST_F(Wg2StorageTest, OriginalVersionTwoBackupIncludesCommittedWalPages)
{
    test::RawDatabase writer(path_);
    writer.execute("PRAGMA journal_mode = WAL; PRAGMA wal_autocheckpoint = 0");
    writer.execute(schema_two);
    writer.execute(schema_two_data);
    ASSERT_GT(std::filesystem::file_size(path_.string() + "-wal"), 0);
    SqliteStore store(path_);
    ASSERT_EQ(store.load().size(), 3);
    const auto files = backups();
    ASSERT_EQ(files.size(), 1);
    expectLegacy(files.front());
}

TEST_F(Wg2StorageTest, InvalidHistoricalStateOrNewFlagDoesNotGainAMeaningDuringMigration)
{
    createLegacy();
    for (const auto &corruption : {"UPDATE flags SET state = 'future' WHERE name = 'build'",
                                   "UPDATE flags SET name = 'entry-deny-message' WHERE name = 'build'",
                                   "UPDATE flags SET name = 'tnt' WHERE name = 'build'",
                                   "UPDATE flag_groups SET name = 'entry-deny-message' WHERE name = 'build'"}) {
        SCOPED_TRACE(corruption);
        executeRaw(corruption);
        EXPECT_THROW(SqliteStore{path_}, std::runtime_error);
        expectLegacy(path_);
        EXPECT_TRUE(backups().empty());
        executeRaw("DELETE FROM flags; DELETE FROM flag_groups; DELETE FROM members; DELETE FROM regions");
        executeRaw(schema_two_data);
    }
}

TEST_F(Wg2StorageTest, InvalidHistoricalHierarchyAbortsBeforeCreatingABackup)
{
    createLegacy();
    executeRaw("UPDATE regions SET parent = 'missing' WHERE name = 'child'");
    EXPECT_THROW(SqliteStore{path_}, std::runtime_error);
    EXPECT_EQ(scalar(path_, "PRAGMA user_version"), 2);
    EXPECT_TRUE(backups().empty());
}

TEST_F(Wg2StorageTest, FailedDdlRestoresVersionTwoAndNeverOverwritesAnEarlierBackup)
{
    createLegacy();
    executeRaw("ALTER TABLE flags ADD COLUMN type TEXT NOT NULL DEFAULT 'keep'");
    EXPECT_THROW(SqliteStore{path_}, std::runtime_error);
    expectLegacy(path_);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM flags WHERE type = 'keep'"), 4);
    const auto first_files = backups();
    ASSERT_EQ(first_files.size(), 1);
    expectLegacy(first_files.front());
    executeRaw("ALTER TABLE flags DROP COLUMN type");
    SqliteStore recovered(path_);
    EXPECT_EQ(recovered.load().size(), 3);
    EXPECT_EQ(backups().size(), 2);
    expectLegacy(first_files.front());
    EXPECT_EQ(scalar(first_files.front(), "SELECT COUNT(*) FROM flags WHERE type = 'keep'"), 4);
}

TEST_F(Wg2StorageTest, FailedVersionTwoCommitRollsBackBothColumnChangesAndVersion)
{
    createLegacy();
    test::RawDatabase reader(path_);
    reader.execute("BEGIN; SELECT * FROM flags");
    {
        sqlite::Connection connection(path_, 0);
        EXPECT_THROW(storage::initializeSchema(connection), std::runtime_error);
    }
    reader.execute("ROLLBACK");
    expectLegacy(path_);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM pragma_table_info('flags') WHERE name = 'type'"), 0);
    const auto files = backups();
    ASSERT_EQ(files.size(), 1);
    expectLegacy(files.front());
}

TEST_F(Wg2StorageTest, TypedSnapshotRoundTripsStringsSetsAndLiteralInherit)
{
    const auto original = typedRegion();
    {
        SqliteStore store(path_);
        store.save({original});
    }
    SqliteStore reopened(path_);
    const auto loaded = reopened.load();
    ASSERT_EQ(loaded.size(), 1);
    test::expectRegionEqual(loaded.front(), original);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM flags WHERE type = 'string' AND value = 'inherit'"), 1);
    EXPECT_EQ(scalar(path_, "SELECT COUNT(*) FROM flags WHERE type = 'set'"), 2);
    EXPECT_TRUE(backups().empty());
}

TEST_F(Wg2StorageTest, AnExplicitEmptySetDoesNotBecomeAnUnsetFlag)
{
    SqliteStore store(path_);
    auto original = typedRegion();
    original.flags[Flag::DenySpawn] = value(Flag::DenySpawn, "[]");
    store.save({original});
    auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    test::expectRegionEqual(loaded.front(), original);
    original.flags.erase(Flag::DenySpawn);
    store.save({original});
    loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    EXPECT_FALSE(loaded.front().flags.contains(Flag::DenySpawn));
}

TEST_F(Wg2StorageTest, RemovingATypedValuePreservesItsIndependentGroupAndParent)
{
    SqliteStore store(path_);
    auto parent = typedRegion();
    parent.key.name = "parent";
    parent.kind = RegionKind::Template;
    auto child = typedRegion();
    child.parent = parent.key.name;
    store.save({parent, child});
    child.flags.erase(Flag::EntryDenyMessage);
    store.save({parent, child});
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 2);
    test::expectRegionEqual(loaded[0], parent);
    test::expectRegionEqual(loaded[1], child);
    EXPECT_FALSE(loaded[1].flags.contains(Flag::EntryDenyMessage));
    EXPECT_EQ(loaded[1].flag_groups.at(Flag::EntryDenyMessage), RegionGroup::NonOwners);
}

TEST_F(Wg2StorageTest, LiteralUnsetAndEmptyStringsRemainTypedValuesInStorage)
{
    SqliteStore store(path_);
    auto original = typedRegion();
    for (const auto *text : {"inherit", "--unset", ""}) {
        SCOPED_TRACE(text);
        original.flags[Flag::EntryDenyMessage] = value(Flag::EntryDenyMessage, text);
        store.save({original});
        const auto loaded = store.load();
        ASSERT_EQ(loaded.size(), 1);
        test::expectRegionEqual(loaded.front(), original);
    }
}

TEST_F(Wg2StorageTest, EveryLoadChecksRegisteredTypesNamesAndEncodedValues)
{
    SqliteStore store(path_);
    const auto original = typedRegion();
    for (const auto &corruption :
         {"UPDATE flags SET type = 'future' WHERE name = 'build'",
          "UPDATE flags SET type = 'string' WHERE name = 'build'",
          "UPDATE flags SET type = 'boolean', value = 'true' WHERE name = 'build'",
          "UPDATE flags SET type = 'integer', value = '123' WHERE name = 'build'",
          "UPDATE flags SET type = 'double', value = '1.25' WHERE name = 'build'",
          "UPDATE flags SET type = 'location', value = 'world,Overworld,0,0,0,0,0' WHERE name = 'build'",
          "UPDATE flags SET type = x'00' WHERE name = 'build'", "UPDATE flags SET value = x'00' WHERE name = 'build'",
          "UPDATE flags SET value = 'future-state' WHERE name = 'build'",
          "UPDATE flags SET value = 'minecraft:creeper,' WHERE name = 'deny-spawn'",
          "UPDATE flags SET value = 'not an actor' WHERE name = 'deny-spawn'",
          "UPDATE flags SET value = 'invalid:domain' WHERE name = 'nonplayer-protection-domains'",
          "UPDATE flags SET value = 'domain name' WHERE name = 'nonplayer-protection-domains'",
          "UPDATE flags SET value = replace(hex(zeroblob(33)), '0', 'x') "
          "WHERE name = 'nonplayer-protection-domains'",
          "UPDATE flags SET value = char(10) WHERE name = 'entry-deny-message'",
          "UPDATE flags SET value = 'before' || char(0) || 'after' WHERE name = 'entry-deny-message'",
          "UPDATE flags SET value = replace(hex(zeroblob(5000)), '0', 'x') WHERE name = 'entry-deny-message'",
          "UPDATE flags SET name = 'future-flag' WHERE name = 'build'",
          "UPDATE flags SET region_id = 9999 WHERE name = 'build'"}) {
        SCOPED_TRACE(corruption);
        store.save({original});
        executeRaw(corruption);
        EXPECT_THROW(static_cast<void>(store.load()), std::runtime_error);
        executeRaw("DELETE FROM flags");
    }
    store.save({original});
    const auto recovered = store.load();
    ASSERT_EQ(recovered.size(), 1);
    test::expectRegionEqual(recovered.front(), original);
}

TEST_F(Wg2StorageTest, InvalidTypedSavePreservesTheCommittedSnapshot)
{
    SqliteStore store(path_);
    const auto original = typedRegion();
    store.save({original});
    auto invalid = original;
    invalid.flags[Flag::DenySpawn] = FlagState::Deny;
    EXPECT_THROW(store.save({invalid}), std::runtime_error);
    invalid = original;
    invalid.flags[Flag::EntryDenyMessage] = value(Flag::DenySpawn, "minecraft:creeper");
    EXPECT_THROW(store.save({invalid}), std::runtime_error);
    invalid = original;
    invalid.flags[Flag::EntryDenyMessage] = FlagValue(std::string(maximum_flag_text_bytes + 1, 'x'));
    EXPECT_THROW(store.save({invalid}), std::runtime_error);
    invalid = original;
    invalid.flags[Flag::NonPlayerProtectionDomains] = FlagValue(FlagSet{"invalid:domain"});
    EXPECT_THROW(store.save({invalid}), std::runtime_error);
    invalid = original;
    invalid.flag_groups[Flag::DenySpawn] = RegionGroup::Members;
    EXPECT_THROW(store.save({invalid}), std::runtime_error);
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    test::expectRegionEqual(loaded.front(), original);
}

TEST_F(Wg2StorageTest, FailedTypedWriteRollsBackAllRegionMemberFlagAndGroupChanges)
{
    SqliteStore store(path_);
    const auto original = typedRegion();
    store.save({original});
    executeRaw("CREATE TRIGGER reject_strings BEFORE INSERT ON flags WHEN NEW.type = 'string' "
               "BEGIN SELECT RAISE(ABORT, 'injected write failure'); END");
    auto replacement = original;
    replacement.key.name = "replacement";
    replacement.members = {"new-member"};
    replacement.flag_groups[Flag::Build] = RegionGroup::Owners;
    EXPECT_THROW(store.save({replacement}), std::runtime_error);
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    test::expectRegionEqual(loaded.front(), original);
}

TEST_F(Wg2StorageTest, FailedTypedCommitPreservesTheEntireHierarchyAndAllowsRetry)
{
    auto parent = typedRegion();
    parent.key.name = "parent";
    parent.kind = RegionKind::Template;
    auto child = typedRegion();
    child.parent = parent.key.name;
    SqliteStore store(path_);
    store.save({parent, child});
    test::RawDatabase reader(path_);
    reader.execute("BEGIN; SELECT * FROM flags");
    {
        sqlite::Connection connection(path_, 0);
        sqlite::Transaction transaction(connection, sqlite::TransactionMode::Write);
        connection.execute("DELETE FROM regions");
        EXPECT_THROW(transaction.commit(), std::runtime_error);
    }
    reader.execute("ROLLBACK");
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 2);
    test::expectRegionEqual(loaded[0], parent);
    test::expectRegionEqual(loaded[1], child);
    child.flags[Flag::EntryDenyMessage] = value(Flag::EntryDenyMessage, "New message");
    store.save({parent, child});
    test::expectRegionEqual(store.load()[1], child);
}

}
}
