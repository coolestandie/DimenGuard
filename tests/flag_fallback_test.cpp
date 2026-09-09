#include "dimenguard/region/flag.h"
#include "dimenguard/region/region_manager.h"

#include <array>
#include <gtest/gtest.h>
#include <set>
#include <string>

namespace dimenguard {
namespace {
const DimensionKey dimension{"world", "minecraft:overworld"};
constexpr BlockPosition position{0, 64, 0};
constexpr std::array granular_flags{Flag::BlockBreak, Flag::BlockPlace, Flag::Use,     Flag::UseAnvil,
                                    Flag::Sleep,      Flag::WaterFlow,  Flag::LavaFlow};

Region region(std::string name = "spawn")
{
    return {{dimension, std::move(name)}, {{-16, 0, -16}, {16, 100, 16}}, 0, "owner", {}, {}};
}
bool decide(const RegionManager &manager, Flag flag, std::string_view player = "outsider", bool bypass = false)
{
    return manager.isAllowed(dimension, position, flag, player, bypass);
}
class FlagFallback : public ::testing::TestWithParam<Flag> {};

TEST_P(FlagFallback, UnsetGranularRuleRetainsTheExistingAggregate)
{
    const auto flag = GetParam();
    const auto fallback = flagFallback(flag);
    ASSERT_TRUE(fallback);
    auto protected_region = region();
    RegionManager manager;
    for (const auto state : supportedFlagStates()) {
        protected_region.flags[*fallback] = state;
        manager.replaceAll({protected_region});
        for (const auto player : {"owner", "outsider"}) {
            EXPECT_EQ(decide(manager, flag, player), decide(manager, *fallback, player));
        }
    }
}

TEST_P(FlagFallback, ExplicitGranularDecisionOverridesAggregateThenInheritRestoresIt)
{
    const auto flag = GetParam();
    const auto fallback = *flagFallback(flag);
    auto protected_region = region();
    protected_region.flags[fallback] = FlagState::Deny;
    protected_region.flags[flag] = FlagState::Allow;
    RegionManager manager;
    manager.replaceAll({protected_region});
    EXPECT_TRUE(decide(manager, flag));
    protected_region.flags[flag] = FlagState::Inherit;
    manager.replaceAll({protected_region});
    EXPECT_FALSE(decide(manager, flag));
    EXPECT_TRUE(decide(manager, flag, "outsider", true));
    protected_region.flags[fallback] = FlagState::Allow;
    protected_region.flags[flag] = FlagState::Deny;
    manager.replaceAll({protected_region});
    EXPECT_FALSE(decide(manager, flag, "owner"));
}

TEST_P(FlagFallback, PriorityAndDenyTiesApplyBeforeAggregateFallback)
{
    const auto flag = GetParam();
    auto high = region("high");
    high.priority = 20;
    high.flags[*flagFallback(flag)] = FlagState::Deny;
    auto low = region("low");
    low.flags[flag] = FlagState::Allow;
    RegionManager manager;
    manager.replaceAll({high, low});
    EXPECT_TRUE(decide(manager, flag));
    high.flags[flag] = FlagState::Deny;
    manager.replaceAll({high, low});
    EXPECT_FALSE(decide(manager, flag));
    low.priority = high.priority;
    manager.replaceAll({low, high});
    EXPECT_FALSE(decide(manager, flag));
}

INSTANTIATE_TEST_SUITE_P(GranularRules, FlagFallback, ::testing::ValuesIn(granular_flags));

TEST(FlagRegistry, FallbackChainsAreAcyclicAndStayInTheirScope)
{
    for (const auto flag : supportedFlags()) {
        std::set<Flag> seen{flag};
        auto parent = flagFallback(flag);
        while (parent) {
            ASSERT_TRUE(seen.insert(*parent).second);
            EXPECT_EQ(flagScope(*parent), flagScope(flag));
            parent = flagFallback(*parent);
        }
    }
}

TEST(FlagRegistry, NestedAnvilFallbackRetainsInteractMembershipAndExplicitUseRules)
{
    auto protected_region = region();
    RegionManager manager;
    manager.replaceAll({protected_region});
    EXPECT_TRUE(decide(manager, Flag::UseAnvil, "owner"));
    EXPECT_FALSE(decide(manager, Flag::UseAnvil));
    protected_region.flags[Flag::Interact] = FlagState::Allow;
    manager.replaceAll({protected_region});
    EXPECT_TRUE(decide(manager, Flag::UseAnvil));
    protected_region.flags[Flag::Use] = FlagState::Deny;
    manager.replaceAll({protected_region});
    EXPECT_FALSE(decide(manager, Flag::UseAnvil, "owner"));
}

TEST(FlagRegistry, EnvironmentalGranularityRetainsAggregateRulesWithoutPlayerIdentity)
{
    auto protected_region = region();
    protected_region.flags[Flag::FluidFlow] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({protected_region});
    EXPECT_FALSE(manager.isEnvironmentAllowed(dimension, position, Flag::WaterFlow));
    EXPECT_FALSE(manager.isEnvironmentAllowed(dimension, position, Flag::LavaFlow));
    protected_region.flags[Flag::WaterFlow] = FlagState::Allow;
    manager.replaceAll({protected_region});
    EXPECT_TRUE(manager.isEnvironmentAllowed(dimension, position, Flag::WaterFlow));
    EXPECT_FALSE(manager.isEnvironmentAllowed(dimension, position, Flag::LavaFlow));
}

TEST(FlagRegistry, InvincibilityRequiresExplicitAllowAndNeverAppearsOutsideRegions)
{
    RegionManager manager;
    EXPECT_FALSE(manager.isEnvironmentAllowed(dimension, position, Flag::Invincible));
    auto protected_region = region();
    manager.replaceAll({protected_region});
    EXPECT_FALSE(manager.isEnvironmentAllowed(dimension, position, Flag::Invincible));
    protected_region.flags[Flag::Invincible] = FlagState::Allow;
    manager.replaceAll({protected_region});
    EXPECT_TRUE(manager.isEnvironmentAllowed(dimension, position, Flag::Invincible));
    EXPECT_FALSE(manager.isEnvironmentAllowed(dimension, {100, 64, 100}, Flag::Invincible));
    auto peer = region("peer");
    peer.flags[Flag::Invincible] = FlagState::Deny;
    manager.replaceAll({protected_region, peer});
    EXPECT_FALSE(manager.isEnvironmentAllowed(dimension, position, Flag::Invincible));
    protected_region.priority = 1;
    manager.replaceAll({protected_region, peer});
    EXPECT_TRUE(manager.isEnvironmentAllowed(dimension, position, Flag::Invincible));
}
}
}
