#include "dimenguard/service/region_service.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace dimenguard {
namespace {

Region makeServiceRegion(std::string name = "spawn")
{
    Region region;
    region.key = {{"survival", "minecraft:overworld"}, std::move(name)};
    region.bounds = {{-16, -64, -16}, {15, 319, 15}};
    region.owner = "00000000-0000-4000-8000-000000000001";
    return region;
}

void expectSameRegion(const Region &actual, const Region &expected)
{
    EXPECT_EQ(actual.key, expected.key);
    EXPECT_EQ(actual.bounds, expected.bounds);
    EXPECT_EQ(actual.priority, expected.priority);
    EXPECT_EQ(actual.owner, expected.owner);
    EXPECT_EQ(actual.members, expected.members);
    EXPECT_EQ(actual.flags, expected.flags);
}

void expectServiceError(const std::function<void()> &operation, ServiceErrorCode code)
{
    try {
        operation();
        FAIL() << "Expected a typed service error";
    }
    catch (const ServiceError &error) {
        EXPECT_EQ(error.getCode(), code);
    }
    catch (const std::exception &error) {
        FAIL() << "Expected a typed service error, received: " << error.what();
    }
}

class ServiceTest : public testing::Test {
protected:
    void SetUp() override
    {
        static std::atomic<unsigned int> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        directory_ = std::filesystem::temp_directory_path() /
                     ("dimenguard-service-" + std::to_string(timestamp) + "-" + std::to_string(sequence++));
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

    void expectStored(const Region &expected) const
    {
        SqliteStore store(path_);
        const auto loaded = store.load();
        ASSERT_EQ(loaded.size(), 1);
        expectSameRegion(loaded.front(), expected);
    }

    std::filesystem::path directory_;
    std::filesystem::path path_;
};

TEST_F(ServiceTest, ReopenRestoresCommittedMutations)
{
    auto expected = makeServiceRegion();
    const std::string member = "00000000-0000-4000-8000-000000000002";
    {
        RegionService service(path_);
        EXPECT_TRUE(service.getRegions().getAll().empty());
        service.create(expected);
        service.setPriority(expected.key, 17);
        service.setFlag(expected.key, Flag::Build, FlagState::Allow);
        service.setMember(expected.key, member, true);
        service.rename(expected.key, "safe_spawn");
    }
    expected.key.name = "safe_spawn";
    expected.priority = 17;
    expected.flags[Flag::Build] = FlagState::Allow;
    expected.members.insert(member);
    RegionService reopened(path_);
    const auto *actual = reopened.getRegions().find(expected.key);
    ASSERT_NE(actual, nullptr);
    expectSameRegion(*actual, expected);
    EXPECT_TRUE(reopened.getRegions().isAllowed(expected.key.dimension, {0, 0, 0}, Flag::Build, "outsider"));
}

TEST_F(ServiceTest, DuplicateCreatePreservesExistingRegion)
{
    const auto original = makeServiceRegion();
    RegionService service(path_);
    service.create(original);
    auto duplicate = original;
    duplicate.owner = "different-owner";
    expectServiceError([&] { service.create(duplicate); }, ServiceErrorCode::Exists);
    ASSERT_EQ(service.getRegions().getAll().size(), 1);
    expectSameRegion(service.getRegions().getAll().front(), original);
    expectStored(original);
}

TEST_F(ServiceTest, SameNameInDifferentDimensionsIsIndependent)
{
    const auto original = makeServiceRegion();
    auto other = original;
    other.key.dimension.dimension = "minecraft:nether";
    RegionService service(path_);
    service.create(original);
    service.create(other);
    service.setPriority(original.key, 24);
    service.setFlag(original.key, Flag::Pvp, FlagState::Deny);
    service.setMember(original.key, "trusted-member", true);
    service.rename(original.key, "renamed");
    const auto *unchanged = service.getRegions().find(other.key);
    ASSERT_NE(unchanged, nullptr);
    expectSameRegion(*unchanged, other);
    EXPECT_EQ(service.getRegions().find(original.key), nullptr);
    const RegionKey renamed{original.key.dimension, "renamed"};
    ASSERT_NE(service.getRegions().find(renamed), nullptr);
    EXPECT_FALSE(service.getRegions().isAllowed(renamed.dimension, {0, 0, 0}, Flag::Pvp, "outsider"));
    EXPECT_TRUE(service.getRegions().isAllowed(other.key.dimension, {0, 0, 0}, Flag::Pvp, "outsider"));
}

TEST_F(ServiceTest, ConflictingRenamePreservesBothNamesAndStorage)
{
    const auto original = makeServiceRegion();
    const auto occupied = makeServiceRegion("occupied");
    RegionService service(path_);
    service.create(original);
    service.create(occupied);
    expectServiceError([&] { service.rename(original.key, occupied.key.name); }, ServiceErrorCode::Exists);
    ASSERT_NE(service.getRegions().find(original.key), nullptr);
    ASSERT_NE(service.getRegions().find(occupied.key), nullptr);
    expectSameRegion(*service.getRegions().find(original.key), original);
    expectSameRegion(*service.getRegions().find(occupied.key), occupied);
    RegionService reopened(path_);
    ASSERT_EQ(reopened.getRegions().getAll().size(), 2);
    EXPECT_NE(reopened.getRegions().find(original.key), nullptr);
    EXPECT_NE(reopened.getRegions().find(occupied.key), nullptr);
}

TEST_F(ServiceTest, InvalidNamesAreTypedWithoutChangingState)
{
    const auto original = makeServiceRegion();
    RegionService service(path_);
    service.create(original);
    for (const auto &name : {std::string{}, std::string{"Uppercase"}, std::string{"bad name"}, std::string(65, 'a')}) {
        SCOPED_TRACE(name);
        expectServiceError([&] { service.create(makeServiceRegion(name)); }, ServiceErrorCode::InvalidName);
        expectServiceError([&] { service.rename(original.key, name); }, ServiceErrorCode::InvalidName);
    }
    ASSERT_EQ(service.getRegions().getAll().size(), 1);
    expectSameRegion(service.getRegions().getAll().front(), original);
    expectStored(original);
}

TEST_F(ServiceTest, MissingRegionMutationsAreTyped)
{
    const auto missing = makeServiceRegion().key;
    RegionService service(path_);
    expectServiceError([&] { service.erase(missing); }, ServiceErrorCode::NotFound);
    expectServiceError([&] { service.rename(missing, "new_name"); }, ServiceErrorCode::NotFound);
    expectServiceError([&] { service.setPriority(missing, 10); }, ServiceErrorCode::NotFound);
    expectServiceError([&] { service.setFlag(missing, Flag::Build, FlagState::Allow); }, ServiceErrorCode::NotFound);
    expectServiceError([&] { service.setMember(missing, "member", true); }, ServiceErrorCode::NotFound);
    EXPECT_TRUE(service.getRegions().getAll().empty());
}

TEST_F(ServiceTest, InheritRemovesExplicitOverrideAndUntrustRetainsOwner)
{
    const auto region = makeServiceRegion();
    RegionService service(path_);
    service.create(region);
    service.setFlag(region.key, Flag::Build, FlagState::Allow);
    EXPECT_TRUE(service.getRegions().isAllowed(region.key.dimension, {0, 0, 0}, Flag::Build, "outsider"));
    service.setFlag(region.key, Flag::Build, FlagState::Inherit);
    EXPECT_FALSE(service.getRegions().isAllowed(region.key.dimension, {0, 0, 0}, Flag::Build, "outsider"));
    service.setMember(region.key, "member", true);
    service.setMember(region.key, "member", true);
    service.setMember(region.key, region.owner, true);
    service.setMember(region.key, region.owner, false);
    EXPECT_TRUE(service.getRegions().isAllowed(region.key.dimension, {0, 0, 0}, Flag::Build, region.owner));
    EXPECT_TRUE(service.getRegions().isAllowed(region.key.dimension, {0, 0, 0}, Flag::Build, "member"));
    service.setMember(region.key, "member", false);
    EXPECT_FALSE(service.getRegions().isAllowed(region.key.dimension, {0, 0, 0}, Flag::Build, "member"));
    expectStored(region);
}

TEST_F(ServiceTest, InvalidValuesCannotReachLiveIndexOrStorage)
{
    const auto original = makeServiceRegion();
    RegionService service(path_);
    service.create(original);
    auto invalid = makeServiceRegion("invalid");
    invalid.bounds.min.x = invalid.bounds.max.x + 1;
    EXPECT_THROW(service.create(invalid), std::invalid_argument);
    EXPECT_THROW(service.setFlag(original.key, static_cast<Flag>(999), FlagState::Inherit), std::invalid_argument);
    EXPECT_THROW(service.setFlag(original.key, Flag::Build, static_cast<FlagState>(999)), std::invalid_argument);
    EXPECT_THROW(service.setMember(original.key, "", true), std::invalid_argument);
    EXPECT_THROW(service.setMember(original.key, "", false), std::invalid_argument);
    ASSERT_EQ(service.getRegions().getAll().size(), 1);
    expectSameRegion(service.getRegions().getAll().front(), original);
    expectStored(original);
}

TEST_F(ServiceTest, FailedSqliteReplacementPreservesLiveAndPersistentSnapshot)
{
    const auto original = makeServiceRegion();
    RegionService service(path_);
    service.create(original);
    executeRaw("CREATE TRIGGER reject_region_insert BEFORE INSERT ON regions "
               "BEGIN SELECT RAISE(ABORT, 'injected storage failure'); END");
    EXPECT_THROW(service.rename(original.key, "renamed"), std::runtime_error);
    ASSERT_EQ(service.getRegions().getAll().size(), 1);
    expectSameRegion(service.getRegions().getAll().front(), original);
    EXPECT_FALSE(service.getRegions().isAllowed(original.key.dimension, {0, 0, 0}, Flag::Build, "outsider"));
    expectStored(original);
    executeRaw("DROP TRIGGER reject_region_insert");
    service.rename(original.key, "renamed");
    EXPECT_EQ(service.getRegions().find(original.key), nullptr);
    auto renamed = original;
    renamed.key.name = "renamed";
    expectStored(renamed);
}

TEST_F(ServiceTest, FailedReloadPreservesWorkingProtectionThenRecovers)
{
    auto original = makeServiceRegion();
    original.flags[Flag::Pvp] = FlagState::Deny;
    RegionService service(path_);
    service.create(original);
    executeRaw("UPDATE flags SET state = 'corrupt' WHERE name = 'pvp'");
    EXPECT_THROW(service.reload(), std::runtime_error);
    ASSERT_EQ(service.getRegions().getAll().size(), 1);
    expectSameRegion(service.getRegions().getAll().front(), original);
    EXPECT_FALSE(service.getRegions().isAllowed(original.key.dimension, {0, 0, 0}, Flag::Pvp, "outsider"));
    executeRaw("UPDATE flags SET state = 'allow' WHERE name = 'pvp'");
    service.reload();
    EXPECT_TRUE(service.getRegions().isAllowed(original.key.dimension, {0, 0, 0}, Flag::Pvp, "outsider"));
}

TEST_F(ServiceTest, ErasePersistsAndDropsSpatialEntry)
{
    const auto original = makeServiceRegion();
    RegionService service(path_);
    service.create(original);
    service.erase(original.key);
    EXPECT_TRUE(service.getRegions().getAll().empty());
    EXPECT_TRUE(service.getRegions().query(original.key.dimension, {0, 0, 0}).empty());
    RegionService reopened(path_);
    EXPECT_TRUE(reopened.getRegions().getAll().empty());
}

TEST_F(ServiceTest, RegionLimitIsEnforcedOnCreateAndReloadWithoutDroppingLiveState)
{
    std::vector<Region> snapshot;
    snapshot.reserve(RegionService::MaxRegions + 1);
    for (std::size_t index = 0; index < RegionService::MaxRegions; ++index) {
        snapshot.push_back(makeServiceRegion("region-" + std::to_string(index)));
    }
    SqliteStore store(path_);
    store.save(snapshot);
    RegionService service(path_);
    ASSERT_EQ(service.getRegions().getAll().size(), RegionService::MaxRegions);
    const auto excess = makeServiceRegion("excess");
    expectServiceError([&] { service.create(excess); }, ServiceErrorCode::LimitReached);
    EXPECT_EQ(store.load().size(), RegionService::MaxRegions);
    snapshot.push_back(excess);
    store.save(snapshot);
    expectServiceError([&] { service.reload(); }, ServiceErrorCode::LimitReached);
    EXPECT_EQ(service.getRegions().getAll().size(), RegionService::MaxRegions);
    EXPECT_EQ(service.getRegions().find(excess.key), nullptr);
    expectServiceError([&] { RegionService rejected(path_); }, ServiceErrorCode::LimitReached);
}

}  // namespace
}  // namespace dimenguard
