#include "dimenguard/storage/limits.h"
#include "dimenguard/storage/schema.h"
#include "dimenguard/storage/sqlite_store.h"
#include "support/database_fixture.h"
#include "support/region_assertions.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace dimenguard {
namespace {

Region makeRegion()
{
    Region region;
    region.key = {{"world's survival", "Overworld"}, "spawn"};
    region.bounds = {{-32, -64, -48}, {31, 319, 15}};
    region.priority = -7;
    region.owner = "00000000-0000-4000-8000-000000000001";
    region.members = {"00000000-0000-4000-8000-000000000002", "00000000-0000-4000-8000-000000000003"};
    region.flags = {{Flag::Build, FlagState::Deny},
                    {Flag::Interact, FlagState::Allow},
                    {Flag::ContainerAccess, FlagState::Inherit},
                    {Flag::Pvp, FlagState::Deny}};
    return region;
}

using test::expectRegionEqual;
using StorageTest = test::DatabaseFixture;

TEST_F(StorageTest, NewDatabaseStartsEmpty)
{
    SqliteStore store(path_);
    EXPECT_TRUE(store.load().empty());
}

TEST_F(StorageTest, RoundTripPreservesEveryRegionField)
{
    SqliteStore store(path_);
    const auto region = makeRegion();
    store.save({region});
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    expectRegionEqual(loaded.front(), region);
}

TEST_F(StorageTest, ReopenRestoresCommittedSnapshot)
{
    const auto region = makeRegion();
    {
        SqliteStore store(path_);
        store.save({region});
    }
    SqliteStore reopened(path_);
    const auto loaded = reopened.load();
    ASSERT_EQ(loaded.size(), 1);
    expectRegionEqual(loaded.front(), region);
}

TEST_F(StorageTest, SameNameCanExistAcrossLevelsAndDimensions)
{
    auto overworld = makeRegion();
    auto nether = overworld;
    nether.key.dimension.dimension = "Nether";
    auto other_level = overworld;
    other_level.key.dimension.level = "second world";
    SqliteStore store(path_);
    store.save({overworld, nether, other_level});
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 3);
    for (const auto &expected : {overworld, nether, other_level}) {
        bool found = false;
        for (const auto &actual : loaded) {
            if (actual.key == expected.key) {
                expectRegionEqual(actual, expected);
                found = true;
            }
        }
        EXPECT_TRUE(found);
    }
}

TEST_F(StorageTest, ReplacingSnapshotRemovesStaleMembersAndFlags)
{
    SqliteStore store(path_);
    auto region = makeRegion();
    store.save({region});
    region.members.clear();
    region.flags.clear();
    region.priority = 42;
    store.save({region});
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    expectRegionEqual(loaded.front(), region);
    store.save({});
    EXPECT_TRUE(store.load().empty());
}

TEST_F(StorageTest, InvalidRegionPreservesPreviousSnapshot)
{
    SqliteStore store(path_);
    const auto original = makeRegion();
    store.save({original});
    auto invalid = original;
    invalid.bounds.min.y = invalid.bounds.max.y + 1;
    EXPECT_THROW(store.save({invalid}), std::runtime_error);
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    expectRegionEqual(loaded.front(), original);
}

TEST_F(StorageTest, DuplicateKeyRollsBackEntireReplacement)
{
    SqliteStore store(path_);
    const auto original = makeRegion();
    store.save({original});
    auto replacement = original;
    replacement.key.name = "replacement";
    replacement.members.clear();
    EXPECT_THROW(store.save({replacement, replacement}), std::runtime_error);
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    expectRegionEqual(loaded.front(), original);
    store.save({replacement});
    const auto recovered = store.load();
    ASSERT_EQ(recovered.size(), 1);
    expectRegionEqual(recovered.front(), replacement);
}

TEST_F(StorageTest, FutureSchemaIsRejectedWithoutErasingRegions)
{
    const auto original = makeRegion();
    {
        SqliteStore store(path_);
        store.save({original});
    }
    executeRaw("PRAGMA user_version = 99");
    EXPECT_THROW(SqliteStore{path_}, std::runtime_error);
    executeRaw("PRAGMA user_version = " + std::to_string(storage::schema_version));
    SqliteStore reopened(path_);
    const auto loaded = reopened.load();
    ASSERT_EQ(loaded.size(), 1);
    expectRegionEqual(loaded.front(), original);
}

TEST_F(StorageTest, EveryOperationChecksSchemaVersionAfterConnectionWasOpened)
{
    SqliteStore store(path_);
    const auto original = makeRegion();
    store.save({original});
    for (const auto version : {0, 1, 2, 99}) {
        executeRaw("PRAGMA user_version = " + std::to_string(version));
        EXPECT_THROW(static_cast<void>(store.load()), std::runtime_error);
        EXPECT_THROW(store.save({}), std::runtime_error);
        executeRaw("PRAGMA user_version = " + std::to_string(storage::schema_version));
        const auto loaded = store.load();
        ASSERT_EQ(loaded.size(), 1);
        expectRegionEqual(loaded.front(), original);
    }
    store.save({});
    EXPECT_TRUE(store.load().empty());
}

TEST_F(StorageTest, OversizedSavePreservesExistingSnapshot)
{
    SqliteStore store(path_);
    const auto original = makeRegion();
    store.save({original});
    const std::vector<Region> oversized(storage::max_regions + 1, original);
    EXPECT_THROW(store.save(oversized), storage::SnapshotLimitError);
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    expectRegionEqual(loaded.front(), original);
}

TEST_F(StorageTest, OversizedLoadIsRejectedBeforeRegionRowsAreMaterialized)
{
    SqliteStore store(path_);
    executeRaw(
        "WITH RECURSIVE ids(id) AS (SELECT 1 UNION ALL SELECT id + 1 FROM ids WHERE id < " +
        std::to_string(storage::max_regions + 1) +
        ") "
        "INSERT INTO regions (level, dimension, name, min_x, min_y, min_z, max_x, max_y, max_z, priority, owner) "
        "SELECT 'world', 'minecraft:overworld', 'region-' || id, 0, 0, 0, 'invalid-coordinate', 0, 0, 0, 'owner' "
        "FROM ids");
    EXPECT_THROW(static_cast<void>(store.load()), storage::SnapshotLimitError);
    executeRaw("DELETE FROM regions WHERE id > 1");
    EXPECT_THROW(static_cast<void>(store.load()), std::runtime_error);
    executeRaw("UPDATE regions SET max_x = 0");
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    EXPECT_EQ(loaded.front().key.name, "region-1");
}

TEST_F(StorageTest, UnknownStoredFlagIsRejectedWithoutDiscardingIt)
{
    SqliteStore store(path_);
    store.save({makeRegion()});
    executeRaw("UPDATE flags SET name = 'future-flag' WHERE name = 'build'");
    EXPECT_THROW(static_cast<void>(store.load()), std::runtime_error);
    EXPECT_THROW(static_cast<void>(store.load()), std::runtime_error);
    executeRaw("UPDATE flags SET name = 'build' WHERE name = 'future-flag'");
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    expectRegionEqual(loaded.front(), makeRegion());
}

TEST_F(StorageTest, InvalidStoredCoordinateDoesNotGetTruncated)
{
    SqliteStore store(path_);
    store.save({makeRegion()});
    executeRaw("UPDATE regions SET max_x = 2147483648");
    EXPECT_THROW(static_cast<void>(store.load()), std::runtime_error);
}

TEST_F(StorageTest, OrphanedMemberIsRejected)
{
    SqliteStore store(path_);
    executeRaw("INSERT INTO members (region_id, identity) VALUES (12345, 'missing-region-member')");
    EXPECT_THROW(static_cast<void>(store.load()), std::runtime_error);
}

TEST_F(StorageTest, UnversionedExistingDatabaseIsNotRepurposed)
{
    executeRaw("CREATE TABLE unrelated (value TEXT); INSERT INTO unrelated VALUES ('keep me')");
    EXPECT_THROW(SqliteStore{path_}, std::runtime_error);
    executeRaw("UPDATE unrelated SET value = 'still here'");
}

TEST_F(StorageTest, CreatesParentDirectoryAndSupportsUnicodePaths)
{
    path_ = directory_ / std::filesystem::path(u8"amatista-\u00f1") / "regions.sqlite3";
    SqliteStore store(path_);
    store.save({makeRegion()});
    const auto loaded = store.load();
    ASSERT_EQ(loaded.size(), 1);
    expectRegionEqual(loaded.front(), makeRegion());
}

}
}
