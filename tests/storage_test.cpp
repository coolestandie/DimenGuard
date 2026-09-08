#include "dimenguard/storage/sqlite_store.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <system_error>
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

void expectRegionEqual(const Region &actual, const Region &expected)
{
    EXPECT_EQ(actual.key, expected.key);
    EXPECT_EQ(actual.bounds, expected.bounds);
    EXPECT_EQ(actual.priority, expected.priority);
    EXPECT_EQ(actual.owner, expected.owner);
    EXPECT_EQ(actual.members, expected.members);
    EXPECT_EQ(actual.flags, expected.flags);
}

class StorageTest : public testing::Test {
protected:
    void SetUp() override
    {
        static std::atomic<unsigned int> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        directory_ = std::filesystem::temp_directory_path() /
                     ("dimenguard-storage-" + std::to_string(timestamp) + "-" + std::to_string(sequence++));
        ASSERT_TRUE(std::filesystem::create_directory(directory_));
        path_ = directory_ / "regions.sqlite3";
    }

    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
        EXPECT_FALSE(error) << error.message();
    }

    void executeRaw(const char *sql) const
    {
        sqlite3 *database = nullptr;
        const auto encoded_path = path_.u8string();
        const std::string filename(encoded_path.begin(), encoded_path.end());
        const auto result = sqlite3_open(filename.c_str(), &database);
        const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> connection(database, sqlite3_close);
        ASSERT_EQ(result, SQLITE_OK);
        ASSERT_EQ(sqlite3_exec(database, sql, nullptr, nullptr, nullptr), SQLITE_OK) << sqlite3_errmsg(database);
    }

    std::filesystem::path directory_;
    std::filesystem::path path_;
};

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
    executeRaw("PRAGMA user_version = 1");
    SqliteStore reopened(path_);
    const auto loaded = reopened.load();
    ASSERT_EQ(loaded.size(), 1);
    expectRegionEqual(loaded.front(), original);
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

}  // namespace
}  // namespace dimenguard
