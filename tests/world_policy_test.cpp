#include "dimenguard/region/flag.h"
#include "dimenguard/region/region_manager.h"
#include "dimenguard/storage/sqlite_store.h"
#include "support/database_fixture.h"
#include "support/region_assertions.h"

#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dimenguard {
namespace {

const DimensionKey overworld{"survival", "minecraft:overworld"};
const DimensionKey nether{"survival", "minecraft:nether"};
const DimensionKey second_world{"creative", "minecraft:overworld"};
constexpr BlockPosition inside{0, 0, 0};
constexpr BlockPosition outside{17, 0, 0};
const auto environment_flags = [] {
    std::vector<Flag> flags;
    for (const auto flag : supportedFlags()) {
        if (flagScope(flag) == FlagScope::Environment && flagDefault(flag) == FlagDefault::Allow) {
            flags.push_back(flag);
        }
    }
    return flags;
}();

Region makeRegion(std::string name = "spawn", DimensionKey dimension = overworld)
{
    Region region;
    region.key = {std::move(dimension), std::move(name)};
    region.bounds = {{-16, -64, -16}, {16, 100, 16}};
    region.owner = "00000000-0000-4000-8000-000000000001";
    return region;
}

class EnvironmentalRules : public ::testing::TestWithParam<Flag> {};

TEST_P(EnvironmentalRules, DefaultsAllowWithoutRequiringAnOwnerIdentity)
{
    auto region = makeRegion();
    region.flags[Flag::Build] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({region});
    EXPECT_TRUE(manager.isEnvironmentAllowed(overworld, inside, GetParam()));
    EXPECT_EQ(flagDefault(GetParam()), FlagDefault::Allow);
    EXPECT_EQ(flagScope(GetParam()), FlagScope::Environment);
    EXPECT_FALSE(manager.isAllowed(overworld, inside, Flag::Build, region.owner));
}

TEST_P(EnvironmentalRules, ExplicitHighestTierWinsWithDenyWinningTies)
{
    auto low = makeRegion("low");
    low.priority = -10;
    low.flags[GetParam()] = FlagState::Deny;
    auto high = makeRegion("high");
    high.priority = 10;
    high.flags[GetParam()] = FlagState::Allow;
    auto peer = makeRegion("peer");
    peer.priority = 10;
    peer.flags[GetParam()] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({low, high});
    EXPECT_TRUE(manager.isEnvironmentAllowed(overworld, inside, GetParam()));
    manager.replaceAll({low, high, peer});
    EXPECT_FALSE(manager.isEnvironmentAllowed(overworld, inside, GetParam()));
    manager.replaceAll({peer, high, low});
    EXPECT_FALSE(manager.isEnvironmentAllowed(overworld, inside, GetParam()));
    peer.priority = 9;
    manager.replaceAll({low, peer, high});
    EXPECT_TRUE(manager.isEnvironmentAllowed(overworld, inside, GetParam()));
}

TEST_P(EnvironmentalRules, InheritedAndMissingDecisionsFallThroughThenDefaultAllow)
{
    auto low = makeRegion("low");
    low.flags[GetParam()] = FlagState::Deny;
    auto high = makeRegion("high");
    high.priority = 100;
    high.flags[GetParam()] = FlagState::Inherit;
    RegionManager manager;
    manager.replaceAll({high, low});
    EXPECT_FALSE(manager.isEnvironmentAllowed(overworld, inside, GetParam()));
    high.flags.clear();
    manager.replaceAll({high, low});
    EXPECT_FALSE(manager.isEnvironmentAllowed(overworld, inside, GetParam()));
    low.flags[GetParam()] = FlagState::Allow;
    manager.replaceAll({high, low});
    EXPECT_TRUE(manager.isEnvironmentAllowed(overworld, inside, GetParam()));
    low.flags[GetParam()] = FlagState::Inherit;
    manager.replaceAll({high, low});
    EXPECT_TRUE(manager.isEnvironmentAllowed(overworld, inside, GetParam()));
}

TEST_P(EnvironmentalRules, InclusiveBoundariesDoNotLeakAcrossDimensionsOrWorlds)
{
    auto region = makeRegion();
    region.flags[GetParam()] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({region, makeRegion("spawn", nether), makeRegion("spawn", second_world)});
    EXPECT_FALSE(manager.isEnvironmentAllowed(overworld, region.bounds.min, GetParam()));
    EXPECT_FALSE(manager.isEnvironmentAllowed(overworld, region.bounds.max, GetParam()));
    EXPECT_TRUE(manager.isEnvironmentAllowed(nether, inside, GetParam()));
    EXPECT_TRUE(manager.isEnvironmentAllowed(second_world, inside, GetParam()));
    for (const auto point :
         std::array<BlockPosition, 6>{{{-17, 0, 0}, {17, 0, 0}, {0, -65, 0}, {0, 101, 0}, {0, 0, -17}, {0, 0, 17}}}) {
        EXPECT_TRUE(manager.isEnvironmentAllowed(overworld, point, GetParam()));
    }
}

INSTANTIATE_TEST_SUITE_P(AllEnvironmentFlags, EnvironmentalRules, ::testing::ValuesIn(environment_flags),
                         [](const ::testing::TestParamInfo<Flag> &info) {
                             auto name = std::string(flagName(info.param));
                             std::replace(name.begin(), name.end(), '-', '_');
                             return name;
                         });

TEST(WorldFlagPolicy, OriginalMembershipDefaultsRemainIntact)
{
    const auto region = makeRegion();
    RegionManager manager;
    manager.replaceAll({region});
    for (const auto flag : {Flag::Build, Flag::Interact, Flag::ContainerAccess}) {
        EXPECT_EQ(flagDefault(flag), FlagDefault::Members);
        EXPECT_FALSE(manager.isAllowed(overworld, inside, flag, "outsider"));
        EXPECT_TRUE(manager.isAllowed(overworld, inside, flag, region.owner));
    }
    EXPECT_TRUE(manager.isAllowed(overworld, inside, Flag::Pvp, "outsider"));
    EXPECT_TRUE(manager.isTransitionAllowed(overworld, outside, overworld, inside, "outsider"));
    EXPECT_TRUE(manager.isTransitionAllowed(overworld, inside, overworld, outside, "outsider"));
}

TEST(WorldFlagPolicy, EnvironmentEntryPointRejectsPlayerTransitionAndUnknownFlags)
{
    RegionManager manager;
    for (const auto flag : {Flag::Build, Flag::Interact, Flag::ContainerAccess, Flag::Pvp, Flag::Entry, Flag::Exit,
                            static_cast<Flag>(999)}) {
        EXPECT_THROW(static_cast<void>(manager.isEnvironmentAllowed(overworld, inside, flag)), std::invalid_argument);
    }
    manager.replaceAll({makeRegion()});
    for (const auto flag : {Flag::Build, Flag::Entry, Flag::Exit}) {
        EXPECT_THROW(static_cast<void>(manager.isEnvironmentAllowed(overworld, inside, flag)), std::invalid_argument);
    }
}

TEST(TransitionRules, EntryAndExitAreIndependent)
{
    auto region = makeRegion();
    region.flags[Flag::Entry] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({region});
    EXPECT_FALSE(manager.isTransitionAllowed(overworld, outside, overworld, inside, "outsider"));
    EXPECT_TRUE(manager.isTransitionAllowed(overworld, inside, overworld, outside, "outsider"));
    region.flags = {{Flag::Exit, FlagState::Deny}};
    manager.replaceAll({region});
    EXPECT_TRUE(manager.isTransitionAllowed(overworld, outside, overworld, inside, "outsider"));
    EXPECT_FALSE(manager.isTransitionAllowed(overworld, inside, overworld, outside, "outsider"));
}

TEST(TransitionRules, StationaryAndInternalMovesDoNotCrossDeniedBoundaries)
{
    auto region = makeRegion();
    region.flags = {{Flag::Entry, FlagState::Deny}, {Flag::Exit, FlagState::Deny}};
    RegionManager manager;
    manager.replaceAll({region});
    EXPECT_TRUE(manager.isTransitionAllowed(overworld, inside, overworld, inside, "outsider"));
    EXPECT_TRUE(manager.isTransitionAllowed(overworld, region.bounds.min, overworld, region.bounds.max, "outsider"));
    EXPECT_TRUE(manager.isTransitionAllowed(overworld, outside, overworld, {18, 0, 0}, "outsider"));
}

TEST(TransitionRules, InclusiveNegativeBoundaryBlocksEntryAndExit)
{
    auto region = makeRegion();
    region.flags = {{Flag::Entry, FlagState::Deny}, {Flag::Exit, FlagState::Deny}};
    RegionManager manager;
    manager.replaceAll({region});
    EXPECT_FALSE(manager.isTransitionAllowed(overworld, {-17, -64, -16}, overworld, {-16, -64, -16}, "outsider"));
    EXPECT_FALSE(manager.isTransitionAllowed(overworld, {-16, -64, -16}, overworld, {-17, -64, -16}, "outsider"));
    EXPECT_TRUE(manager.isTransitionAllowed(overworld, {-16, -64, -16}, overworld, {-15, -64, -16}, "outsider"));
}

TEST(TransitionRules, OwnerAndMembersDoNotOverrideExplicitDenial)
{
    auto region = makeRegion();
    region.members.insert("member");
    region.flags = {{Flag::Entry, FlagState::Deny}, {Flag::Exit, FlagState::Deny}};
    RegionManager manager;
    manager.replaceAll({region});
    for (const auto &player : {region.owner, std::string{"member"}, std::string{"outsider"}}) {
        EXPECT_FALSE(manager.isTransitionAllowed(overworld, outside, overworld, inside, player));
        EXPECT_FALSE(manager.isTransitionAllowed(overworld, inside, overworld, outside, player));
        EXPECT_TRUE(manager.isTransitionAllowed(overworld, outside, overworld, inside, player, true));
        EXPECT_TRUE(manager.isTransitionAllowed(overworld, inside, overworld, outside, player, true));
    }
}

TEST(TransitionRules, SameNamedRegionsInDifferentDimensionsAreDistinctCrossings)
{
    auto source = makeRegion();
    auto destination = makeRegion("spawn", nether);
    destination.flags[Flag::Entry] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({source, destination});
    EXPECT_FALSE(manager.isTransitionAllowed(overworld, inside, nether, inside, "outsider"));
    EXPECT_TRUE(manager.isTransitionAllowed(nether, inside, overworld, inside, "outsider"));
    destination.flags.clear();
    source.flags[Flag::Exit] = FlagState::Deny;
    manager.replaceAll({source, destination});
    EXPECT_FALSE(manager.isTransitionAllowed(overworld, inside, nether, inside, "outsider"));
}

TEST(TransitionRules, SameDimensionNameInDifferentWorldsIsNotStationary)
{
    auto destination = makeRegion("spawn", second_world);
    destination.flags[Flag::Entry] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({makeRegion(), destination});
    EXPECT_FALSE(manager.isTransitionAllowed(overworld, inside, second_world, inside, "outsider"));
    EXPECT_TRUE(manager.isTransitionAllowed(second_world, inside, overworld, inside, "outsider"));
}

TEST(TransitionRules, BothDepartureAndArrivalMustAllowTheTransition)
{
    auto source = makeRegion("source");
    source.bounds = {{-20, 0, 0}, {-10, 10, 10}};
    source.flags[Flag::Exit] = FlagState::Deny;
    auto destination = makeRegion("destination");
    destination.bounds = {{10, 0, 0}, {20, 10, 10}};
    destination.flags[Flag::Entry] = FlagState::Allow;
    RegionManager manager;
    manager.replaceAll({source, destination});
    EXPECT_FALSE(manager.isTransitionAllowed(overworld, {-15, 5, 5}, overworld, {15, 5, 5}, "outsider"));
    source.flags[Flag::Exit] = FlagState::Allow;
    destination.flags[Flag::Entry] = FlagState::Deny;
    manager.replaceAll({source, destination});
    EXPECT_FALSE(manager.isTransitionAllowed(overworld, {-15, 5, 5}, overworld, {15, 5, 5}, "outsider"));
    destination.flags[Flag::Entry] = FlagState::Allow;
    manager.replaceAll({source, destination});
    EXPECT_TRUE(manager.isTransitionAllowed(overworld, {-15, 5, 5}, overworld, {15, 5, 5}, "outsider"));
}

TEST(TransitionRules, EnteredAndExitedSubsetsRetainPriorityAndDenyTies)
{
    for (const auto flag : {Flag::Entry, Flag::Exit}) {
        const auto from = flag == Flag::Entry ? outside : inside;
        const auto to = flag == Flag::Entry ? inside : outside;
        auto low = makeRegion("low");
        low.flags[flag] = FlagState::Deny;
        auto high = makeRegion("high");
        high.priority = 5;
        high.flags[flag] = FlagState::Allow;
        auto peer = makeRegion("peer");
        peer.priority = 5;
        peer.flags[flag] = FlagState::Deny;
        RegionManager manager;
        manager.replaceAll({high, low});
        EXPECT_TRUE(manager.isTransitionAllowed(overworld, from, overworld, to, "outsider"));
        manager.replaceAll({peer, low, high});
        EXPECT_FALSE(manager.isTransitionAllowed(overworld, from, overworld, to, "outsider"));
        high.flags[flag] = FlagState::Inherit;
        peer.flags.clear();
        manager.replaceAll({low, peer, high});
        EXPECT_FALSE(manager.isTransitionAllowed(overworld, from, overworld, to, "outsider"));
        low.flags[flag] = FlagState::Inherit;
        manager.replaceAll({low, peer, high});
        EXPECT_TRUE(manager.isTransitionAllowed(overworld, from, overworld, to, "outsider"));
    }
}

TEST(TransitionRules, UnchangedHighPriorityParentCannotMaskNewChildEntryOrExit)
{
    auto parent = makeRegion("parent");
    parent.priority = 100;
    parent.flags = {{Flag::Entry, FlagState::Allow}, {Flag::Exit, FlagState::Allow}};
    auto child = makeRegion("child");
    child.bounds = {{0, 0, 0}, {2, 2, 2}};
    child.flags = {{Flag::Entry, FlagState::Deny}, {Flag::Exit, FlagState::Deny}};
    RegionManager manager;
    manager.replaceAll({parent, child});
    EXPECT_FALSE(manager.isTransitionAllowed(overworld, {-1, 1, 1}, overworld, {1, 1, 1}, "outsider"));
    EXPECT_FALSE(manager.isTransitionAllowed(overworld, {1, 1, 1}, overworld, {-1, 1, 1}, "outsider"));
    EXPECT_TRUE(manager.isTransitionAllowed(overworld, {1, 1, 1}, overworld, {2, 2, 2}, "outsider"));
}

TEST(TransitionRules, UnchangedDeniedRegionDoesNotPreventLeavingAnotherRegion)
{
    auto parent = makeRegion("parent");
    parent.priority = 100;
    parent.flags = {{Flag::Entry, FlagState::Deny}, {Flag::Exit, FlagState::Deny}};
    auto child = makeRegion("child");
    child.bounds = {{0, 0, 0}, {2, 2, 2}};
    RegionManager manager;
    manager.replaceAll({parent, child});
    EXPECT_TRUE(manager.isTransitionAllowed(overworld, {-1, 1, 1}, overworld, {1, 1, 1}, "outsider"));
    EXPECT_TRUE(manager.isTransitionAllowed(overworld, {1, 1, 1}, overworld, {-1, 1, 1}, "outsider"));
}

using WorldFlagsStorageTest = test::DatabaseFixture;

TEST_F(WorldFlagsStorageTest, EveryRegisteredFlagAndStateSurvivesDatabaseReopen)
{
    ASSERT_EQ(supportedFlags().size(), 26);
    std::vector<Region> expected;
    for (const auto state : supportedFlagStates()) {
        auto region = makeRegion(std::string(stateName(state)));
        for (const auto flag : supportedFlags()) {
            region.flags[flag] = state;
        }
        expected.push_back(std::move(region));
    }
    {
        SqliteStore store(path_);
        store.save(expected);
    }
    SqliteStore reopened(path_);
    const auto actual = reopened.load();
    ASSERT_EQ(actual.size(), expected.size());
    for (const auto &region : expected) {
        const auto found = std::ranges::find(actual, region.key, &Region::key);
        ASSERT_NE(found, actual.end());
        test::expectRegionEqual(*found, region);
    }
}

}
}
