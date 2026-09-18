#include "dimenguard/service/region_service.h"
#include "dimenguard/storage/limits.h"
#include "support/database_fixture.h"
#include "support/region_assertions.h"

#include <filesystem>
#include <functional>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
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

using test::expectRegionEqual;

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

class ServiceTest : public test::DatabaseFixture {
protected:
    void expectStored(const Region &expected) const
    {
        SqliteStore store(path_);
        const auto loaded = store.load();
        ASSERT_EQ(loaded.size(), 1);
        expectRegionEqual(loaded.front(), expected);
    }
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
    expectRegionEqual(*actual, expected);
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
    expectRegionEqual(service.getRegions().getAll().front(), original);
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
    expectRegionEqual(*unchanged, other);
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
    expectRegionEqual(*service.getRegions().find(original.key), original);
    expectRegionEqual(*service.getRegions().find(occupied.key), occupied);
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
    expectRegionEqual(service.getRegions().getAll().front(), original);
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
    expectRegionEqual(service.getRegions().getAll().front(), original);
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
    expectRegionEqual(service.getRegions().getAll().front(), original);
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
    executeRaw("UPDATE flags SET value = 'corrupt' WHERE name = 'pvp'");
    EXPECT_THROW(service.reload(), std::runtime_error);
    ASSERT_EQ(service.getRegions().getAll().size(), 1);
    expectRegionEqual(service.getRegions().getAll().front(), original);
    EXPECT_FALSE(service.getRegions().isAllowed(original.key.dimension, {0, 0, 0}, Flag::Pvp, "outsider"));
    executeRaw("UPDATE flags SET value = 'allow' WHERE name = 'pvp'");
    service.reload();
    EXPECT_TRUE(service.getRegions().isAllowed(original.key.dimension, {0, 0, 0}, Flag::Pvp, "outsider"));
}

TEST_F(ServiceTest, ChangedSchemaBlocksReloadAndWritesWithoutReplacingLiveRegions)
{
    const auto original = makeServiceRegion();
    RegionService service(path_);
    service.create(original);
    executeRaw("PRAGMA user_version = 99");
    EXPECT_THROW(service.reload(), std::runtime_error);
    EXPECT_THROW(service.setPriority(original.key, 27), std::runtime_error);
    ASSERT_EQ(service.getRegions().getAll().size(), 1);
    expectRegionEqual(service.getRegions().getAll().front(), original);
    executeRaw("PRAGMA user_version = 3");
    expectStored(original);
    service.reload();
    expectRegionEqual(service.getRegions().getAll().front(), original);
}

TEST_F(ServiceTest, FailedCommitRollsBackStorageAndKeepsTheLiveSnapshot)
{
    const auto original = makeServiceRegion();
    RegionService service(path_);
    service.create(original);
    test::RawDatabase reader(path_);
    reader.execute("BEGIN; SELECT COUNT(*) FROM regions");
    // The reader permits the reserved write lock but prevents COMMIT's exclusive lock.
    EXPECT_THROW(service.rename(original.key, "renamed"), std::runtime_error);
    ASSERT_EQ(service.getRegions().getAll().size(), 1);
    expectRegionEqual(service.getRegions().getAll().front(), original);
    EXPECT_FALSE(service.getRegions().isAllowed(original.key.dimension, {0, 0, 0}, Flag::Build, "outsider"));
    reader.execute("ROLLBACK");
    expectStored(original);
    service.rename(original.key, "renamed");
    auto renamed = original;
    renamed.key.name = "renamed";
    expectStored(renamed);
}

TEST_F(ServiceTest, RegionNamesRevisionChangesOnlyAfterSuccessfulCatalogMutations)
{
    const auto original = makeServiceRegion();
    RegionService service(path_);
    auto revision = service.getRegionNamesRevision();
    service.create(original);
    EXPECT_EQ(service.getRegionNamesRevision(), ++revision);
    service.setPriority(original.key, 20);
    service.setFlag(original.key, Flag::Pvp, FlagState::Deny);
    service.setMember(original.key, "friend", true);
    EXPECT_EQ(service.getRegionNamesRevision(), revision);
    EXPECT_THROW(service.create(original), ServiceError);
    EXPECT_EQ(service.getRegionNamesRevision(), revision);
    executeRaw("PRAGMA user_version = 99");
    EXPECT_THROW(service.rename(original.key, "renamed"), std::runtime_error);
    EXPECT_THROW(service.reload(), std::runtime_error);
    EXPECT_EQ(service.getRegionNamesRevision(), revision);
    executeRaw("PRAGMA user_version = 3");
    service.rename(original.key, "renamed");
    EXPECT_EQ(service.getRegionNamesRevision(), ++revision);
    service.reload();
    EXPECT_EQ(service.getRegionNamesRevision(), ++revision);
    service.erase({original.key.dimension, "renamed"});
    EXPECT_EQ(service.getRegionNamesRevision(), ++revision);
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

TEST_F(ServiceTest, GlobalAndTemplateCreationPersistWithoutInventingPhysicalBounds)
{
    const auto plot = makeServiceRegion();
    RegionService service(path_);
    service.createGlobal(plot.key.dimension, "global-owner");
    service.createTemplate(plot.key.dimension, "template", "template-owner");
    service.create(plot);
    const RegionKey global{plot.key.dimension, std::string(global_region_name)};
    const RegionKey region_template{plot.key.dimension, "template"};
    service.setFlag(global, Flag::Pvp, FlagState::Deny);
    service.setFlag(region_template, Flag::Build, FlagState::Allow);
    EXPECT_FALSE(service.getRegions().isAllowed(plot.key.dimension, {100, 10, 100}, Flag::Pvp, "visitor"));
    EXPECT_TRUE(service.getRegions().isAllowed(plot.key.dimension, {100, 10, 100}, Flag::Build, "visitor"));
    EXPECT_FALSE(service.getRegions().isAllowed(plot.key.dimension, {0, 0, 0}, Flag::Build, "visitor"));
    service.setParent(plot.key, region_template);
    RegionService reopened(path_);
    ASSERT_EQ(reopened.getRegions().getAll().size(), 3);
    EXPECT_EQ(reopened.getRegions().find(global)->kind, RegionKind::Global);
    EXPECT_EQ(reopened.getRegions().find(global)->priority, std::numeric_limits<int>::min());
    EXPECT_EQ(reopened.getRegions().find(region_template)->kind, RegionKind::Template);
    EXPECT_TRUE(reopened.getRegions().isAllowed(plot.key.dimension, {0, 0, 0}, Flag::Build, "visitor"));
    EXPECT_EQ(reopened.getRegions().query(plot.key.dimension, {0, 0, 0}).size(), 2);
    expectServiceError([&] { service.createGlobal(plot.key.dimension, "different-owner"); }, ServiceErrorCode::Exists);
    expectServiceError([&] { service.rename(global, "world"); }, ServiceErrorCode::InvalidRegionType);
    expectServiceError([&] { service.setPriority(global, 0); }, ServiceErrorCode::InvalidRegionType);
}

TEST_F(ServiceTest, LegacyGlobalNameRemainsACuboidUntilExplicitlyRenamed)
{
    auto legacy = makeServiceRegion(std::string(global_region_name));
    SqliteStore(path_).save({legacy});
    RegionService service(path_);
    EXPECT_EQ(service.getRegions().find(legacy.key)->kind, RegionKind::Cuboid);
    EXPECT_TRUE(service.getRegions().query(legacy.key.dimension, {100, 0, 100}).empty());
    expectServiceError([&] { service.createGlobal(legacy.key.dimension, "owner"); }, ServiceErrorCode::Exists);
    service.rename(legacy.key, "legacy_global");
    service.createGlobal(legacy.key.dimension, "owner");
    EXPECT_EQ(service.getRegions().getAll().size(), 2);
    expectServiceError([&] { service.createTemplate(legacy.key.dimension, std::string(global_region_name), "owner"); },
                       ServiceErrorCode::InvalidName);
}

TEST_F(ServiceTest, ParentRenameUpdatesDirectChildrenAtomicallyAndKeepsDimensionsSeparate)
{
    auto parent = makeServiceRegion("parent");
    auto child = makeServiceRegion("child");
    auto second = makeServiceRegion("second");
    auto other_parent = parent;
    other_parent.key.dimension.dimension = "minecraft:nether";
    auto other_child = child;
    other_child.key.dimension = other_parent.key.dimension;
    RegionService service(path_);
    for (const auto &region : {parent, child, second, other_parent, other_child}) {
        service.create(region);
    }
    service.setParent(child.key, parent.key);
    service.setParent(second.key, parent.key);
    service.setParent(other_child.key, other_parent.key);
    const auto revision = service.getRegionNamesRevision();
    service.rename(parent.key, "renamed");
    EXPECT_EQ(service.getRegionNamesRevision(), revision + 1);
    EXPECT_EQ(service.getRegions().find(child.key)->parent, "renamed");
    EXPECT_EQ(service.getRegions().find(second.key)->parent, "renamed");
    EXPECT_EQ(service.getRegions().find(other_child.key)->parent, "parent");
    RegionService reopened(path_);
    EXPECT_EQ(reopened.getRegions().find(child.key)->parent, "renamed");
    EXPECT_EQ(reopened.getRegions().find(second.key)->parent, "renamed");
    EXPECT_EQ(reopened.getRegions().find(other_child.key)->parent, "parent");
    expectServiceError([&] { service.erase({parent.key.dimension, "renamed"}); }, ServiceErrorCode::HasChildren);
    service.setParent(child.key, std::nullopt);
    service.setParent(second.key, std::nullopt);
    service.erase({parent.key.dimension, "renamed"});
    EXPECT_EQ(service.getRegions().find({parent.key.dimension, "renamed"}), nullptr);
}

TEST_F(ServiceTest, InvalidHierarchyMutationsKeepBothSnapshotsAndRevision)
{
    const auto parent = makeServiceRegion("parent");
    const auto child = makeServiceRegion("child");
    RegionService service(path_);
    service.create(parent);
    service.create(child);
    service.setParent(child.key, parent.key);
    const auto revision = service.getRegionNamesRevision();
    const RegionKey missing{parent.key.dimension, "missing"};
    const RegionKey other{{"survival", "minecraft:nether"}, "parent"};
    expectServiceError([&] { service.setParent(child.key, missing); }, ServiceErrorCode::InvalidHierarchy);
    expectServiceError([&] { service.setParent(child.key, other); }, ServiceErrorCode::InvalidHierarchy);
    expectServiceError([&] { service.setParent(parent.key, child.key); }, ServiceErrorCode::InvalidHierarchy);
    expectServiceError([&] { service.setParent(parent.key, parent.key); }, ServiceErrorCode::InvalidHierarchy);
    expectServiceError([&] { service.setPriority(parent.key, 1); }, ServiceErrorCode::InvalidHierarchy);
    expectServiceError([&] { service.setPriority(child.key, -1); }, ServiceErrorCode::InvalidHierarchy);
    expectServiceError([&] { service.setParent(missing, std::nullopt); }, ServiceErrorCode::NotFound);
    EXPECT_EQ(service.getRegionNamesRevision(), revision);
    RegionService reopened(path_);
    expectRegionEqual(*reopened.getRegions().find(parent.key), parent);
    auto expected_child = child;
    expected_child.parent = parent.key.name;
    expectRegionEqual(*reopened.getRegions().find(child.key), expected_child);
    expectRegionEqual(*service.getRegions().find(child.key), expected_child);
    service.setPriority(child.key, 1);
    service.setPriority(parent.key, 1);
    EXPECT_EQ(service.getRegions().find(parent.key)->priority, 1);
}

TEST_F(ServiceTest, FailedParentRenameKeepsStoredLinksAndLiveInheritedProtection)
{
    auto parent = makeServiceRegion("parent");
    parent.flags[Flag::Pvp] = FlagState::Deny;
    auto child = makeServiceRegion("child");
    child.bounds = {{100, 0, 100}, {110, 10, 110}};
    RegionService service(path_);
    service.create(parent);
    service.create(child);
    service.setParent(child.key, parent.key);
    const auto revision = service.getRegionNamesRevision();
    executeRaw("CREATE TRIGGER reject_region_insert BEFORE INSERT ON regions "
               "BEGIN SELECT RAISE(ABORT, 'injected hierarchy write failure'); END");
    EXPECT_THROW(service.rename(parent.key, "renamed"), std::runtime_error);
    EXPECT_EQ(service.getRegionNamesRevision(), revision);
    EXPECT_EQ(service.getRegions().find(child.key)->parent, "parent");
    EXPECT_FALSE(service.getRegions().isAllowed(child.key.dimension, {105, 0, 105}, Flag::Pvp, "visitor"));
    RegionService reopened(path_);
    EXPECT_EQ(reopened.getRegions().find(child.key)->parent, "parent");
    EXPECT_FALSE(reopened.getRegions().isAllowed(child.key.dimension, {105, 0, 105}, Flag::Pvp, "visitor"));
}

TEST_F(ServiceTest, GroupAndPassthroughOverridesRoundTripAndCanBeClearedIndependently)
{
    const auto parent = makeServiceRegion("parent");
    auto child = makeServiceRegion("child");
    child.owner = "child-owner";
    RegionService service(path_);
    service.create(parent);
    service.create(child);
    service.setParent(child.key, parent.key);
    service.setFlag(parent.key, Flag::Pvp, FlagState::Deny);
    service.setFlagGroup(parent.key, Flag::Pvp, RegionGroup::NonMembers);
    EXPECT_TRUE(service.getRegions().isAllowed(child.key.dimension, {0, 0, 0}, Flag::Pvp, child.owner));
    EXPECT_FALSE(service.getRegions().isAllowed(child.key.dimension, {0, 0, 0}, Flag::Pvp, "visitor"));
    service.setFlagGroup(child.key, Flag::Pvp, RegionGroup::All);
    EXPECT_FALSE(service.getRegions().isAllowed(child.key.dimension, {0, 0, 0}, Flag::Pvp, child.owner));
    service.setFlag(child.key, Flag::Pvp, FlagState::Allow);
    service.setFlag(child.key, Flag::Pvp, FlagState::Inherit);
    EXPECT_EQ(service.getRegions().find(child.key)->flag_groups.at(Flag::Pvp), RegionGroup::All);
    service.setFlagGroup(child.key, Flag::Pvp, std::nullopt);
    service.setPassthrough(parent.key, FlagState::Allow);
    RegionService reopened(path_);
    EXPECT_TRUE(reopened.getRegions().isAllowed(child.key.dimension, {0, 0, 0}, Flag::Pvp, child.owner));
    EXPECT_TRUE(reopened.getRegions().isAllowed(child.key.dimension, {0, 0, 0}, Flag::Build, "visitor"));
    service.setPassthrough(parent.key, FlagState::Inherit);
    EXPECT_FALSE(service.getRegions().isAllowed(child.key.dimension, {0, 0, 0}, Flag::Build, "visitor"));
    EXPECT_THROW(service.setFlagGroup(child.key, Flag::Explosions, RegionGroup::Members), std::invalid_argument);
    EXPECT_THROW(service.setFlagGroup(child.key, Flag::Pvp, static_cast<RegionGroup>(999)), std::invalid_argument);
    EXPECT_THROW(service.setPassthrough(child.key, static_cast<FlagState>(999)), std::invalid_argument);
}

TEST_F(ServiceTest, RejectedCyclicSaveAndFailedReloadPreserveWorkingHierarchy)
{
    auto parent = makeServiceRegion("parent");
    parent.flags[Flag::Pvp] = FlagState::Deny;
    const auto child = makeServiceRegion("child");
    RegionService service(path_);
    service.create(parent);
    service.create(child);
    service.setParent(child.key, parent.key);
    auto corrupt = service.getRegions().getAll();
    corrupt.front().parent = child.key.name;
    EXPECT_THROW(SqliteStore(path_).save(corrupt), std::runtime_error);
    executeRaw("UPDATE regions SET parent = 'missing' WHERE name = 'child'");
    EXPECT_THROW(service.reload(), std::runtime_error);
    EXPECT_EQ(service.getRegions().find(child.key)->parent, "parent");
    EXPECT_FALSE(service.getRegions().isAllowed(child.key.dimension, {0, 0, 0}, Flag::Pvp, "visitor"));
    executeRaw("UPDATE regions SET parent = 'parent' WHERE name = 'child'");
    service.reload();
    EXPECT_FALSE(service.getRegions().find(parent.key)->parent.has_value());
    EXPECT_FALSE(service.getRegions().isAllowed(child.key.dimension, {0, 0, 0}, Flag::Pvp, "visitor"));
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
    EXPECT_THROW(store.save(snapshot), storage::SnapshotLimitError);
    executeRaw(
        "INSERT INTO regions (level, dimension, name, min_x, min_y, min_z, max_x, max_y, max_z, priority, owner) "
        "VALUES ('survival', 'minecraft:overworld', 'excess', -16, -64, -16, 15, 319, 15, 0, "
        "'00000000-0000-4000-8000-000000000001')");
    EXPECT_THROW(static_cast<void>(store.load()), storage::SnapshotLimitError);
    expectServiceError([&] { service.reload(); }, ServiceErrorCode::LimitReached);
    EXPECT_EQ(service.getRegions().getAll().size(), RegionService::MaxRegions);
    EXPECT_EQ(service.getRegions().find(excess.key), nullptr);
    expectServiceError([&] { RegionService rejected(path_); }, ServiceErrorCode::LimitReached);
}

}
}
