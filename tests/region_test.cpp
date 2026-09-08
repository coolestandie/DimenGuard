#include "dimenguard/region/region.h"
#include "dimenguard/region/region_manager.h"

#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dimenguard {
namespace {

const DimensionKey overworld{"test-level", "minecraft:overworld"};
const DimensionKey nether{"test-level", "minecraft:nether"};
const DimensionKey other_level{"other-level", "minecraft:overworld"};

Region makeRegion(std::string name = "spawn", DimensionKey dimension = overworld)
{
    Region region;
    region.key = {std::move(dimension), std::move(name)};
    region.bounds = Bounds::between({-16, -64, -16}, {16, 100, 16});
    region.owner = "owner-id";
    return region;
}

TEST(RegionBounds, NormalizesEachAxisAndIncludesEdges)
{
    const auto bounds = Bounds::between({15, -64, -17}, {-16, 319, 32});
    EXPECT_EQ(bounds.min, (BlockPosition{-16, -64, -17}));
    EXPECT_EQ(bounds.max, (BlockPosition{15, 319, 32}));
    EXPECT_TRUE(bounds.contains(bounds.min));
    EXPECT_TRUE(bounds.contains(bounds.max));
    EXPECT_TRUE(bounds.contains({-1, 0, -1}));
    for (const auto &outside :
         std::array<BlockPosition, 6>{{{-17, 0, 0}, {16, 0, 0}, {0, -65, 0}, {0, 320, 0}, {0, 0, -18}, {0, 0, 33}}}) {
        EXPECT_FALSE(bounds.contains(outside));
    }
}

TEST(RegionBounds, SupportsSingleBlockAndFullIntegerRange)
{
    const BlockPosition point{-1, -1, -1};
    const auto single = Bounds::between(point, point);
    EXPECT_TRUE(single.contains(point));
    EXPECT_FALSE(single.contains({0, -1, -1}));

    const int minimum = std::numeric_limits<int>::min();
    const int maximum = std::numeric_limits<int>::max();
    const auto full = Bounds::between({maximum, minimum, maximum}, {minimum, maximum, minimum});
    EXPECT_TRUE(full.contains({minimum, maximum, minimum}));
    EXPECT_TRUE(full.contains({maximum, minimum, maximum}));
}

TEST(RegionFlags, NamesRoundTripAndRejectUnknownValues)
{
    for (const auto flag : {Flag::Build, Flag::Interact, Flag::ContainerAccess, Flag::Pvp}) {
        EXPECT_EQ(parseFlag(flagName(flag)), flag);
    }
    for (const auto state : {FlagState::Inherit, FlagState::Allow, FlagState::Deny}) {
        EXPECT_EQ(parseState(stateName(state)), state);
    }
    EXPECT_EQ(flagName(Flag::ContainerAccess), "container-access");
    EXPECT_FALSE(parseFlag("BUILD"));
    EXPECT_FALSE(parseFlag("unsupported"));
    EXPECT_FALSE(parseState("true"));
    EXPECT_THROW(static_cast<void>(flagName(static_cast<Flag>(999))), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(stateName(static_cast<FlagState>(999))), std::invalid_argument);
}

TEST(RegionValidation, RejectsInvalidNames)
{
    for (const auto &name : {std::string{}, std::string{"Spawn"}, std::string{"has space"}, std::string{"../spawn"},
                             std::string(65, 'a')}) {
        EXPECT_THROW(validateRegion(makeRegion(name)), std::invalid_argument);
    }
    EXPECT_NO_THROW(validateRegion(makeRegion("spawn_2-east")));
    EXPECT_NO_THROW(validateRegion(makeRegion(std::string(64, 'a'))));
}

TEST(RegionValidation, RejectsInvalidBoundsAndIdentities)
{
    auto region = makeRegion();
    region.bounds.min.x = region.bounds.max.x + 1;
    EXPECT_THROW(validateRegion(region), std::invalid_argument);
    region = makeRegion();
    region.bounds.min.y = region.bounds.max.y + 1;
    EXPECT_THROW(validateRegion(region), std::invalid_argument);
    region = makeRegion();
    region.bounds.min.z = region.bounds.max.z + 1;
    EXPECT_THROW(validateRegion(region), std::invalid_argument);
    region = makeRegion();
    region.owner.clear();
    EXPECT_THROW(validateRegion(region), std::invalid_argument);
    region = makeRegion();
    region.key.dimension.level.clear();
    EXPECT_THROW(validateRegion(region), std::invalid_argument);
    region = makeRegion();
    region.key.dimension.dimension.clear();
    EXPECT_THROW(validateRegion(region), std::invalid_argument);
    region = makeRegion();
    region.members.insert("");
    EXPECT_THROW(validateRegion(region), std::invalid_argument);
    region = makeRegion();
    region.flags[static_cast<Flag>(999)] = FlagState::Allow;
    EXPECT_THROW(validateRegion(region), std::invalid_argument);
    region = makeRegion();
    region.flags[Flag::Build] = static_cast<FlagState>(999);
    EXPECT_THROW(validateRegion(region), std::invalid_argument);
}

TEST(RegionManager, SeparatesLevelsAndDimensions)
{
    RegionManager manager;
    auto main = makeRegion();
    main.flags[Flag::Build] = FlagState::Deny;
    auto alternate = makeRegion("spawn", nether);
    alternate.flags[Flag::Build] = FlagState::Allow;
    auto separate = makeRegion("spawn", other_level);
    separate.flags[Flag::Build] = FlagState::Allow;
    manager.replaceAll({main, alternate, separate});
    ASSERT_EQ(manager.query(overworld, {0, 0, 0}).size(), 1);
    ASSERT_EQ(manager.query(nether, {0, 0, 0}).size(), 1);
    ASSERT_EQ(manager.query(other_level, {0, 0, 0}).size(), 1);
    EXPECT_FALSE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Build, "stranger-id"));
    EXPECT_TRUE(manager.isAllowed(nether, {0, 0, 0}, Flag::Build, "stranger-id"));
    EXPECT_TRUE(manager.isAllowed(other_level, {0, 0, 0}, Flag::Build, "stranger-id"));
    EXPECT_NE(manager.find(main.key), nullptr);
    EXPECT_EQ(manager.find({overworld, "missing"}), nullptr);
}

TEST(RegionManager, FailedReplacementKeepsExistingStateAndIndex)
{
    RegionManager manager;
    const auto original = makeRegion();
    manager.replaceAll({original});
    const auto *previous = manager.find(original.key);
    EXPECT_THROW(manager.replaceAll({makeRegion("duplicate"), makeRegion("duplicate")}), std::invalid_argument);
    EXPECT_EQ(manager.find(original.key), previous);
    auto invalid = makeRegion("new-region");
    invalid.owner.clear();
    EXPECT_THROW(manager.replaceAll({invalid}), std::invalid_argument);
    EXPECT_EQ(manager.find(original.key), previous);
    ASSERT_EQ(manager.query(overworld, {-16, -64, -16}).size(), 1);
    EXPECT_EQ(manager.query(overworld, {-16, -64, -16}).front(), previous);
}

TEST(RegionManager, EmptyReplacementRemovesStaleEntries)
{
    RegionManager manager;
    const auto region = makeRegion();
    manager.replaceAll({region});
    manager.replaceAll({});
    EXPECT_TRUE(manager.getAll().empty());
    EXPECT_TRUE(manager.query(overworld, {0, 0, 0}).empty());
    EXPECT_EQ(manager.find(region.key), nullptr);
    EXPECT_TRUE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Build, "stranger-id"));
}

TEST(RegionManager, ReplacementReindexesMovedRegionsAndDimensions)
{
    RegionManager manager;
    auto region = makeRegion();
    manager.replaceAll({region});
    region.key.dimension = nether;
    region.bounds = Bounds::between({500, 0, 500}, {510, 10, 510});
    manager.replaceAll({region});
    EXPECT_TRUE(manager.query(overworld, {0, 0, 0}).empty());
    EXPECT_TRUE(manager.query(nether, {0, 0, 0}).empty());
    ASSERT_EQ(manager.query(nether, {505, 5, 505}).size(), 1);
    EXPECT_EQ(manager.query(nether, {505, 5, 505}).front()->key, region.key);
}

TEST(RegionManager, CopiedIndexUsesItsOwnRegionStorage)
{
    RegionManager original;
    original.replaceAll({makeRegion()});
    auto copy = original;
    original.replaceAll({});
    const auto matches = copy.query(overworld, {0, 0, 0});
    ASSERT_EQ(matches.size(), 1);
    EXPECT_EQ(matches.front(), &copy.getAll().front());
    EXPECT_FALSE(copy.isAllowed(overworld, {0, 0, 0}, Flag::Build, "stranger-id"));
}

TEST(RegionManager, IndexMatchesBruteForceAcrossNegativeCoordinatesAndOverlaps)
{
    std::mt19937 random(48311);
    std::uniform_int_distribution<int> coordinate(-500, 500);
    std::uniform_int_distribution<int> extent(0, 120);
    std::uniform_int_distribution<int> priority(-5, 5);
    std::vector<Region> regions;
    for (int index = 0; index < 300; ++index) {
        auto region = makeRegion("region-" + std::to_string(index), index % 2 == 0 ? overworld : nether);
        const BlockPosition min{coordinate(random), coordinate(random), coordinate(random)};
        region.bounds = {min, {min.x + extent(random), min.y + extent(random), min.z + extent(random)}};
        region.priority = priority(random);
        regions.push_back(std::move(region));
    }
    RegionManager manager;
    manager.replaceAll(regions);
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const auto &dimension = attempt % 2 == 0 ? overworld : nether;
        const BlockPosition point{coordinate(random), coordinate(random), coordinate(random)};
        std::vector<const Region *> expected;
        for (const auto &region : manager.getAll()) {
            if (region.key.dimension == dimension && region.bounds.contains(point)) {
                expected.push_back(&region);
            }
        }
        std::ranges::sort(expected, [](const Region *a, const Region *b) {
            return a->priority != b->priority ? a->priority > b->priority : a->key.name < b->key.name;
        });
        EXPECT_EQ(manager.query(dimension, point), expected);
    }
    for (const auto &region : manager.getAll()) {
        const auto matches = manager.query(region.key.dimension, region.bounds.min);
        EXPECT_NE(std::ranges::find(matches, &region), matches.end());
    }
}

TEST(RegionManager, WorldSizedRegionsDoNotExpandIntoChunkEntries)
{
    const int minimum = std::numeric_limits<int>::min();
    const int maximum = std::numeric_limits<int>::max();
    std::vector<Region> regions;
    for (int index = 0; index < 20; ++index) {
        auto region = makeRegion("full-" + std::to_string(index));
        region.bounds = {{minimum, minimum, minimum}, {maximum, maximum, maximum}};
        regions.push_back(std::move(region));
    }
    RegionManager manager;
    manager.replaceAll(regions);
    EXPECT_EQ(manager.query(overworld, {minimum, minimum, minimum}).size(), 20);
    EXPECT_EQ(manager.query(overworld, {maximum, maximum, maximum}).size(), 20);
}

TEST(RegionPolicy, OutsideRegionsIsAllowed)
{
    RegionManager manager;
    manager.replaceAll({makeRegion()});
    for (const auto flag : {Flag::Build, Flag::Interact, Flag::ContainerAccess, Flag::Pvp}) {
        EXPECT_TRUE(manager.isAllowed(overworld, {17, 0, 0}, flag, "stranger-id"));
    }
}

TEST(RegionPolicy, DefaultAccessRequiresMembershipWhilePvpAllowsEveryone)
{
    RegionManager manager;
    auto region = makeRegion();
    region.members.insert("member-id");
    manager.replaceAll({region});
    for (const auto flag : {Flag::Build, Flag::Interact, Flag::ContainerAccess}) {
        EXPECT_TRUE(manager.isAllowed(overworld, {0, 0, 0}, flag, "owner-id"));
        EXPECT_TRUE(manager.isAllowed(overworld, {0, 0, 0}, flag, "member-id"));
        EXPECT_FALSE(manager.isAllowed(overworld, {0, 0, 0}, flag, "stranger-id"));
    }
    EXPECT_TRUE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Pvp, "stranger-id"));
}

TEST(RegionPolicy, ExplicitDenyAppliesToMembersAndOwnerUnlessBypassed)
{
    RegionManager manager;
    auto region = makeRegion();
    region.flags[Flag::Build] = FlagState::Deny;
    region.flags[Flag::Pvp] = FlagState::Deny;
    region.members.insert("member-id");
    manager.replaceAll({region});
    for (const auto player : {"owner-id", "member-id", "stranger-id"}) {
        EXPECT_FALSE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Build, player));
        EXPECT_FALSE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Pvp, player));
        EXPECT_TRUE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Build, player, true));
    }
}

TEST(RegionPolicy, HighestExplicitPriorityWinsAndInheritedTiersFallThrough)
{
    RegionManager manager;
    auto low = makeRegion("low");
    low.priority = -10;
    low.flags[Flag::Build] = FlagState::Deny;
    auto middle = makeRegion("middle");
    middle.priority = 0;
    middle.flags[Flag::Build] = FlagState::Allow;
    auto high = makeRegion("high");
    high.priority = 50;
    high.flags[Flag::Build] = FlagState::Inherit;
    manager.replaceAll({low, middle, high});
    EXPECT_TRUE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Build, "stranger-id"));
    middle.flags[Flag::Build] = FlagState::Inherit;
    manager.replaceAll({low, middle, high});
    EXPECT_FALSE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Build, "owner-id"));
    high.flags[Flag::Build] = FlagState::Allow;
    manager.replaceAll({low, middle, high});
    EXPECT_TRUE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Build, "stranger-id"));
}

TEST(RegionPolicy, EqualPriorityDenyWinsRegardlessOfInsertionOrder)
{
    RegionManager manager;
    auto allow = makeRegion("allow");
    allow.flags[Flag::Build] = FlagState::Allow;
    auto deny = makeRegion("deny");
    deny.flags[Flag::Build] = FlagState::Deny;
    auto inherit = makeRegion("inherit");
    inherit.flags[Flag::Build] = FlagState::Inherit;
    manager.replaceAll({allow, deny, inherit});
    EXPECT_FALSE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Build, "owner-id"));
    manager.replaceAll({inherit, deny, allow});
    EXPECT_FALSE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Build, "owner-id"));
}

TEST(RegionPolicy, DefaultsRequireMembershipOfAllTopPriorityRegionsOnly)
{
    RegionManager manager;
    auto low = makeRegion("low");
    low.owner = "unrelated-owner";
    low.priority = -1;
    auto first = makeRegion("first");
    first.members.insert("member-id");
    auto second = makeRegion("second");
    second.owner = "second-owner";
    manager.replaceAll({low, first, second});
    EXPECT_FALSE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Build, "member-id"));
    second.members.insert("member-id");
    manager.replaceAll({low, first, second});
    EXPECT_TRUE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Build, "member-id"));
}

TEST(RegionPolicy, FlagsAreIndependentAndInvalidFlagsAreRejected)
{
    RegionManager manager;
    auto region = makeRegion();
    region.flags[Flag::Build] = FlagState::Allow;
    region.flags[Flag::Interact] = FlagState::Deny;
    manager.replaceAll({region});
    EXPECT_TRUE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Build, "stranger-id"));
    EXPECT_FALSE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Interact, "owner-id"));
    EXPECT_TRUE(manager.isAllowed(overworld, {0, 0, 0}, Flag::ContainerAccess, "owner-id"));
    EXPECT_THROW(static_cast<void>(manager.isAllowed(overworld, {0, 0, 0}, static_cast<Flag>(999), "owner-id", true)),
                 std::invalid_argument);
}

TEST(RegionPolicy, ExtremePrioritiesAreOrderedWithoutOverflow)
{
    auto low = makeRegion("low");
    low.priority = std::numeric_limits<int>::min();
    low.flags[Flag::Build] = FlagState::Deny;
    auto high = makeRegion("high");
    high.priority = std::numeric_limits<int>::max();
    high.flags[Flag::Build] = FlagState::Allow;
    RegionManager manager;
    manager.replaceAll({low, high});
    ASSERT_EQ(manager.query(overworld, {0, 0, 0}).size(), 2);
    EXPECT_EQ(manager.query(overworld, {0, 0, 0}).front()->key.name, "high");
    EXPECT_TRUE(manager.isAllowed(overworld, {0, 0, 0}, Flag::Build, "stranger-id"));
}

}  // namespace
}  // namespace dimenguard
