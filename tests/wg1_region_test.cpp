#include "dimenguard/region/flag.h"
#include "dimenguard/region/protection_policy.h"
#include "dimenguard/region/region.h"
#include "dimenguard/region/region_context.h"
#include "dimenguard/region/region_index.h"
#include "dimenguard/region/region_manager.h"

#include <array>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dimenguard {
namespace {

const DimensionKey dimension{"world", "minecraft:overworld"};
const DimensionKey other_dimension{"world", "minecraft:nether"};
constexpr BlockPosition inside{0, 0, 0};
constexpr BlockPosition outside{100, 0, 0};

Region region(std::string name = "plot", RegionKind kind = RegionKind::Cuboid)
{
    Region value;
    value.key = {dimension, std::move(name)};
    value.bounds = {{-10, -10, -10}, {10, 10, 10}};
    value.owner = "owner-" + value.key.name;
    value.kind = kind;
    if (kind == RegionKind::Global) {
        value.key.name = global_region_name;
        value.priority = std::numeric_limits<int>::min();
    }
    return value;
}

bool allowed(const RegionManager &manager, Flag flag = Flag::Build, std::string_view player = "outsider")
{
    return manager.isAllowed(dimension, inside, flag, player);
}

TEST(RegionHierarchyValidation, NamesKindsAndGroupsHaveCanonicalRoundTrips)
{
    for (const auto kind : {RegionKind::Cuboid, RegionKind::Global, RegionKind::Template}) {
        EXPECT_EQ(parseRegionKind(regionKindName(kind)), kind);
    }
    for (const auto group : supportedRegionGroups()) {
        EXPECT_EQ(parseRegionGroup(regionGroupName(group)), group);
    }
    EXPECT_FALSE(parseRegionKind("GLOBAL"));
    EXPECT_FALSE(parseRegionGroup("non-members"));
    EXPECT_THROW(static_cast<void>(regionKindName(static_cast<RegionKind>(99))), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(regionGroupName(static_cast<RegionGroup>(99))), std::invalid_argument);
}

TEST(RegionHierarchyValidation, RejectsMissingCrossDimensionAndSelfParents)
{
    auto child = region("child");
    child.parent = "parent";
    EXPECT_THROW(validateRegions(std::array{child}), std::invalid_argument);
    auto parent = region("parent");
    parent.key.dimension = other_dimension;
    EXPECT_THROW(validateRegions(std::array{child, parent}), std::invalid_argument);
    child.parent = child.key.name;
    EXPECT_THROW(validateRegions(std::array{child}), std::invalid_argument);
    child.parent = "PARENT";
    EXPECT_THROW(validateRegion(child), std::invalid_argument);
    child.parent = "";
    EXPECT_THROW(validateRegion(child), std::invalid_argument);
}

TEST(RegionHierarchyValidation, RejectsCyclesAndHigherPriorityParents)
{
    auto parent = region("parent");
    auto child = region("child");
    child.parent = parent.key.name;
    parent.parent = child.key.name;
    EXPECT_THROW(validateRegions(std::array{parent, child}), std::invalid_argument);
    parent.parent.reset();
    parent.priority = 1;
    EXPECT_THROW(validateRegions(std::array{parent, child}), std::invalid_argument);
    child.priority = 1;
    EXPECT_NO_THROW(validateRegions(std::array{parent, child}));
    child.priority = 2;
    EXPECT_NO_THROW(validateRegions(std::array{parent, child}));
}

TEST(RegionHierarchyValidation, AcceptsMaximumDepthAndRejectsOneExtraRegion)
{
    std::vector<Region> regions;
    for (std::size_t index = 0; index < maximum_region_depth; ++index) {
        auto value = region("node-" + std::to_string(index), RegionKind::Template);
        if (index != 0) {
            value.parent = regions.back().key.name;
        }
        regions.push_back(std::move(value));
    }
    EXPECT_NO_THROW(validateRegions(regions));
    auto extra = region("extra");
    extra.parent = regions.back().key.name;
    regions.push_back(std::move(extra));
    EXPECT_THROW(validateRegions(regions), std::invalid_argument);
}

TEST(RegionHierarchyValidation, GlobalIdentityAndPriorityAreFixedAndCannotJoinHierarchy)
{
    auto global = region("unused", RegionKind::Global);
    EXPECT_NO_THROW(validateRegion(global));
    global.priority = 0;
    EXPECT_THROW(validateRegion(global), std::invalid_argument);
    global = region("unused", RegionKind::Global);
    global.key.name = "not-global";
    EXPECT_THROW(validateRegion(global), std::invalid_argument);
    global = region("unused", RegionKind::Global);
    global.parent = "parent";
    EXPECT_THROW(validateRegion(global), std::invalid_argument);
    global.parent.reset();
    auto child = region("child");
    child.parent = global.key.name;
    EXPECT_THROW(validateRegions(std::array{global, child}), std::invalid_argument);
    EXPECT_THROW(validateRegions(std::array{global, global}), std::invalid_argument);
}

TEST(RegionHierarchyValidation, LegacyGlobalNamedCuboidRemainsSpatial)
{
    auto legacy = region(std::string(global_region_name));
    RegionManager manager;
    EXPECT_NO_THROW(manager.replaceAll({legacy}));
    EXPECT_FALSE(allowed(manager));
    EXPECT_TRUE(manager.query(dimension, outside).empty());
    EXPECT_EQ(manager.getAll().front().kind, RegionKind::Cuboid);
}

TEST(RegionHierarchyValidation, RejectsInvalidPassthroughKindsAndEnvironmentalGroups)
{
    auto value = region();
    value.kind = static_cast<RegionKind>(99);
    EXPECT_THROW(validateRegion(value), std::invalid_argument);
    value = region();
    value.passthrough = static_cast<FlagState>(99);
    EXPECT_THROW(validateRegion(value), std::invalid_argument);
    for (const auto flag : supportedFlags()) {
        value = region();
        value.flag_groups[flag] = RegionGroup::Members;
        if (flagScope(flag) == FlagScope::Environment) {
            EXPECT_THROW(validateRegion(value), std::invalid_argument) << flagName(flag);
        }
        else {
            EXPECT_NO_THROW(validateRegion(value)) << flagName(flag);
        }
        value.flag_groups[flag] = RegionGroup::All;
        EXPECT_NO_THROW(validateRegion(value));
    }
    value.flag_groups[static_cast<Flag>(99)] = RegionGroup::All;
    EXPECT_THROW(validateRegion(value), std::invalid_argument);
    value = region();
    value.flag_groups[Flag::Build] = static_cast<RegionGroup>(99);
    EXPECT_THROW(validateRegion(value), std::invalid_argument);
}

TEST(RegionHierarchy, InheritsParentAndGrandparentOwnersAndMembers)
{
    auto root = region("root", RegionKind::Template);
    root.members.insert("root-member");
    auto parent = region("parent", RegionKind::Template);
    parent.parent = root.key.name;
    parent.members.insert("parent-member");
    auto child = region("child");
    child.parent = parent.key.name;
    child.members.insert("child-member");
    RegionManager manager;
    manager.replaceAll({root, child, parent});
    for (const auto &player : {root.owner, parent.owner, child.owner, std::string{"root-member"},
                               std::string{"parent-member"}, std::string{"child-member"}}) {
        EXPECT_TRUE(allowed(manager, Flag::Build, player)) << player;
    }
    EXPECT_FALSE(allowed(manager));
    EXPECT_FALSE(allowed(manager, Flag::Build, ""));
}

TEST(RegionHierarchy, ChildMembershipDoesNotLeakIntoParentOrSibling)
{
    auto parent = region("parent");
    parent.bounds = {{-20, -20, -20}, {20, 20, 20}};
    auto child = region("child");
    child.parent = parent.key.name;
    auto sibling = region("sibling");
    sibling.parent = parent.key.name;
    sibling.bounds = {{30, 0, 0}, {40, 10, 10}};
    RegionManager manager;
    manager.replaceAll({parent, child, sibling});
    EXPECT_TRUE(allowed(manager, Flag::Build, child.owner));
    EXPECT_FALSE(manager.isAllowed(dimension, {15, 0, 0}, Flag::Build, child.owner));
    EXPECT_FALSE(manager.isAllowed(dimension, {35, 0, 0}, Flag::Build, child.owner));
}

TEST(RegionHierarchy, ClosestFlagWinsAtChildPriorityAndUnrelatedDenyStillWinsTies)
{
    auto root = region("root", RegionKind::Template);
    root.flags[Flag::Pvp] = FlagState::Deny;
    auto parent = region("parent");
    parent.parent = root.key.name;
    parent.flags[Flag::Pvp] = FlagState::Allow;
    auto child = region("child");
    child.parent = parent.key.name;
    child.priority = 10;
    auto unrelated = region("unrelated");
    unrelated.priority = 9;
    unrelated.flags[Flag::Pvp] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({child, unrelated, root, parent});
    EXPECT_TRUE(allowed(manager, Flag::Pvp));
    unrelated.priority = child.priority;
    manager.replaceAll({child, unrelated, root, parent});
    EXPECT_FALSE(allowed(manager, Flag::Pvp));
    child.flags[Flag::Pvp] = FlagState::Deny;
    manager.replaceAll({child, root, parent});
    EXPECT_FALSE(allowed(manager, Flag::Pvp));
    child.flags[Flag::Pvp] = FlagState::Inherit;
    manager.replaceAll({child, root, parent});
    EXPECT_TRUE(allowed(manager, Flag::Pvp));
}

TEST(RegionHierarchy, ExplicitChildAllowOverridesSamePriorityParentDeny)
{
    auto parent = region("parent");
    parent.flags[Flag::Build] = FlagState::Deny;
    auto child = region("child");
    child.parent = parent.key.name;
    child.flags[Flag::Build] = FlagState::Allow;
    RegionManager manager;
    manager.replaceAll({parent, child});
    EXPECT_TRUE(allowed(manager));
    auto unrelated = region("unrelated");
    unrelated.flags[Flag::Build] = FlagState::Deny;
    manager.replaceAll({parent, child, unrelated});
    EXPECT_FALSE(allowed(manager));
}

TEST(RegionHierarchy, PhysicalParentNonmemberRuleUsesChildMembershipAtEveryPriority)
{
    auto parent = region("market");
    parent.flags[Flag::Pvp] = FlagState::Deny;
    parent.flag_groups[Flag::Pvp] = RegionGroup::NonMembers;
    auto child = region("shop");
    child.parent = parent.key.name;
    child.members.insert("shop-member");
    RegionManager manager;
    for (const int priority : {0, 10}) {
        child.priority = priority;
        manager.replaceAll({parent, child});
        EXPECT_TRUE(allowed(manager, Flag::Pvp, child.owner));
        EXPECT_TRUE(allowed(manager, Flag::Pvp, "shop-member"));
        EXPECT_TRUE(allowed(manager, Flag::Pvp, parent.owner));
        EXPECT_FALSE(allowed(manager, Flag::Pvp));
        auto unrelated = region("unrelated");
        unrelated.flags[Flag::Pvp] = FlagState::Deny;
        manager.replaceAll({parent, child, unrelated});
        EXPECT_FALSE(allowed(manager, Flag::Pvp, "shop-member"));
    }
}

TEST(RegionHierarchy, GroupOverrideAndStateOverrideInheritIndependently)
{
    auto parent = region("parent", RegionKind::Template);
    parent.flags[Flag::Pvp] = FlagState::Deny;
    parent.flag_groups[Flag::Pvp] = RegionGroup::NonMembers;
    auto child = region("child");
    child.parent = parent.key.name;
    child.flag_groups[Flag::Pvp] = RegionGroup::All;
    RegionManager manager;
    manager.replaceAll({parent, child});
    EXPECT_FALSE(allowed(manager, Flag::Pvp, child.owner));
    child.flag_groups.clear();
    child.flags[Flag::Pvp] = FlagState::Deny;
    manager.replaceAll({parent, child});
    EXPECT_TRUE(allowed(manager, Flag::Pvp, child.owner));
    EXPECT_FALSE(allowed(manager, Flag::Pvp));
}

TEST(RegionGroups, MemberExceptionPreservesParentDenialForOutsiders)
{
    auto parent = region("parent");
    parent.flags[Flag::Pvp] = FlagState::Deny;
    auto child = region("child");
    child.parent = parent.key.name;
    child.flags[Flag::Pvp] = FlagState::Allow;
    child.flag_groups[Flag::Pvp] = RegionGroup::Members;
    child.members.insert("child-member");
    RegionManager manager;
    for (const int priority : {0, 10}) {
        child.priority = priority;
        manager.replaceAll({parent, child});
        EXPECT_TRUE(allowed(manager, Flag::Pvp, child.owner));
        EXPECT_TRUE(allowed(manager, Flag::Pvp, "child-member"));
        EXPECT_TRUE(allowed(manager, Flag::Pvp, parent.owner));
        EXPECT_FALSE(allowed(manager, Flag::Pvp));
    }
}

TEST(RegionGroups, SkippedInheritedExceptionFallsThroughToGrandparentRule)
{
    auto grandparent = region("grandparent", RegionKind::Template);
    grandparent.flags[Flag::Pvp] = FlagState::Deny;
    auto parent = region("parent");
    parent.parent = grandparent.key.name;
    parent.flags[Flag::Pvp] = FlagState::Allow;
    parent.flag_groups[Flag::Pvp] = RegionGroup::Members;
    auto child = region("child");
    child.parent = parent.key.name;
    child.members.insert("child-member");
    RegionManager manager;
    manager.replaceAll({grandparent, parent, child});
    EXPECT_TRUE(allowed(manager, Flag::Pvp, "child-member"));
    EXPECT_FALSE(allowed(manager, Flag::Pvp));
    child.flag_groups[Flag::Pvp] = RegionGroup::All;
    manager.replaceAll({grandparent, parent, child});
    EXPECT_TRUE(allowed(manager, Flag::Pvp));
}

TEST(RegionGroups, EachGroupSeparatesOwnersMembersAndUnrelatedPlayers)
{
    auto value = region();
    value.members.insert("member");
    value.flags[Flag::Pvp] = FlagState::Deny;
    const std::array<std::pair<RegionGroup, std::array<bool, 3>>, 5> cases{{
        {RegionGroup::All, {false, false, false}},
        {RegionGroup::Members, {false, false, true}},
        {RegionGroup::Owners, {false, true, true}},
        {RegionGroup::NonMembers, {true, true, false}},
        {RegionGroup::NonOwners, {true, false, false}},
    }};
    RegionManager manager;
    for (const auto &[group, expectations] : cases) {
        value.flag_groups[Flag::Pvp] = group;
        manager.replaceAll({value});
        EXPECT_EQ(allowed(manager, Flag::Pvp, value.owner), expectations[0]) << regionGroupName(group);
        EXPECT_EQ(allowed(manager, Flag::Pvp, "member"), expectations[1]) << regionGroupName(group);
        EXPECT_EQ(allowed(manager, Flag::Pvp), expectations[2]) << regionGroupName(group);
    }
}

TEST(RegionKinds, TemplatesDoNotParticipateInSpatialQueriesAndGlobalsAlwaysDo)
{
    auto templated = region("template", RegionKind::Template);
    templated.flags[Flag::Build] = FlagState::Deny;
    auto global = region("unused", RegionKind::Global);
    RegionManager manager;
    manager.replaceAll({templated, global});
    ASSERT_EQ(manager.query(dimension, inside).size(), 1);
    EXPECT_EQ(manager.query(dimension, inside).front()->kind, RegionKind::Global);
    ASSERT_EQ(manager.query(dimension, outside).size(), 1);
    EXPECT_TRUE(manager.query(other_dimension, inside).empty());
    EXPECT_EQ(manager.inDimension(dimension).size(), 2);
    RegionIndex index;
    index.replaceAll(manager.getAll());
    EXPECT_TRUE(index.query(dimension, inside).empty());
}

TEST(RegionKinds, GlobalFallbackIsBelowEvenMinimumPriorityCuboids)
{
    auto global = region("unused", RegionKind::Global);
    global.flags[Flag::Pvp] = FlagState::Deny;
    auto local = region();
    local.priority = std::numeric_limits<int>::min();
    local.flags[Flag::Pvp] = FlagState::Allow;
    RegionManager manager;
    manager.replaceAll({global, local});
    const auto matching = manager.query(dimension, inside);
    ASSERT_EQ(matching.size(), 2);
    EXPECT_EQ(matching.front()->kind, RegionKind::Cuboid);
    EXPECT_TRUE(allowed(manager, Flag::Pvp));
    EXPECT_FALSE(manager.isAllowed(dimension, outside, Flag::Pvp, "outsider"));
    local.flags.clear();
    manager.replaceAll({global, local});
    EXPECT_FALSE(allowed(manager, Flag::Pvp));
}

TEST(RegionKinds, GlobalDefaultsAllowBuildingAndExplicitPassthroughDenyProtectsWilderness)
{
    auto global = region("unused", RegionKind::Global);
    RegionManager manager;
    manager.replaceAll({global});
    EXPECT_TRUE(allowed(manager));
    EXPECT_FALSE(manager.isEnvironmentAllowed(dimension, inside, Flag::Invincible));
    global.passthrough = FlagState::Deny;
    manager.replaceAll({global});
    EXPECT_FALSE(allowed(manager));
    EXPECT_TRUE(allowed(manager, Flag::Build, global.owner));
    auto local = region();
    local.priority = std::numeric_limits<int>::min();
    manager.replaceAll({global, local});
    EXPECT_TRUE(allowed(manager, Flag::Build, local.owner));
    EXPECT_FALSE(allowed(manager));
}

TEST(RegionKinds, GlobalBuildAllowDoesNotOpenOrdinaryProtectedRegions)
{
    auto global = region("unused", RegionKind::Global);
    global.flags[Flag::Build] = FlagState::Allow;
    auto local = region();
    local.priority = std::numeric_limits<int>::min();
    RegionManager manager;
    manager.replaceAll({global, local});
    EXPECT_FALSE(allowed(manager));
    EXPECT_FALSE(allowed(manager, Flag::BlockBreak));
    EXPECT_TRUE(allowed(manager, Flag::Build, local.owner));
    EXPECT_TRUE(manager.isAllowed(dimension, outside, Flag::Build, "outsider"));
    global.passthrough = FlagState::Deny;
    manager.replaceAll({global, local});
    EXPECT_FALSE(manager.isAllowed(dimension, outside, Flag::Build, "outsider"));
}

TEST(RegionKinds, GlobalBuildDenyProtectsWildernessWithoutOverridingLocalMembership)
{
    auto global = region("unused", RegionKind::Global);
    global.flags[Flag::Build] = FlagState::Deny;
    auto local = region();
    local.priority = std::numeric_limits<int>::min();
    RegionManager manager;
    manager.replaceAll({global, local});
    EXPECT_FALSE(allowed(manager));
    EXPECT_TRUE(allowed(manager, Flag::Build, local.owner));
    EXPECT_TRUE(allowed(manager, Flag::BlockPlace, local.owner));
    EXPECT_FALSE(manager.isAllowed(dimension, outside, Flag::Build, local.owner));
    local.passthrough = FlagState::Allow;
    manager.replaceAll({global, local});
    EXPECT_FALSE(allowed(manager, Flag::Build, local.owner));
    local.flags[Flag::Build] = FlagState::Allow;
    manager.replaceAll({global, local});
    EXPECT_TRUE(allowed(manager));
}

TEST(RegionKinds, GlobalOtherFlagsRemainExplicitFallbacksInsideOrdinaryRegions)
{
    auto global = region("unused", RegionKind::Global);
    global.flags[Flag::ContainerAccess] = FlagState::Deny;
    auto local = region();
    RegionManager manager;
    manager.replaceAll({global, local});
    EXPECT_FALSE(allowed(manager, Flag::ContainerAccess, local.owner));
    global.flags[Flag::ContainerAccess] = FlagState::Allow;
    manager.replaceAll({global, local});
    EXPECT_TRUE(allowed(manager, Flag::ContainerAccess));
}

TEST(RegionPassthrough, AffectsMembershipFallbacksButNotExplicitFlagsOrMovement)
{
    auto value = region();
    value.passthrough = FlagState::Allow;
    value.flags[Flag::Entry] = FlagState::Deny;
    value.flags[Flag::Pvp] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({value});
    for (const auto flag : {Flag::Build, Flag::BlockBreak, Flag::BlockPlace, Flag::Interact, Flag::Use, Flag::UseAnvil,
                            Flag::Sleep, Flag::ContainerAccess}) {
        EXPECT_TRUE(allowed(manager, flag)) << flagName(flag);
    }
    EXPECT_FALSE(allowed(manager, Flag::Pvp));
    EXPECT_FALSE(manager.isTransitionAllowed(dimension, outside, dimension, inside, "outsider"));
    value.flags[Flag::Build] = FlagState::Deny;
    manager.replaceAll({value});
    EXPECT_FALSE(allowed(manager));
    EXPECT_FALSE(allowed(manager, Flag::BlockBreak));
}

TEST(RegionPassthrough, SkipsTransparentTierAndInheritsNearestParentSetting)
{
    auto low = region("low");
    auto parent = region("parent", RegionKind::Template);
    parent.passthrough = FlagState::Allow;
    auto high = region("high");
    high.parent = parent.key.name;
    high.priority = 10;
    RegionManager manager;
    manager.replaceAll({parent, low, high});
    EXPECT_FALSE(allowed(manager));
    EXPECT_TRUE(allowed(manager, Flag::Build, low.owner));
    high.passthrough = FlagState::Deny;
    manager.replaceAll({parent, low, high});
    EXPECT_FALSE(allowed(manager, Flag::Build, low.owner));
    EXPECT_TRUE(allowed(manager, Flag::Build, high.owner));
}

TEST(RegionPassthrough, TransparentChildRetainsPhysicalParentMembershipProtection)
{
    auto parent = region("parent");
    auto child = region("child");
    child.parent = parent.key.name;
    child.passthrough = FlagState::Allow;
    RegionManager manager;
    for (const int priority : {0, 10}) {
        child.priority = priority;
        manager.replaceAll({parent, child});
        EXPECT_FALSE(allowed(manager));
        EXPECT_TRUE(allowed(manager, Flag::Build, parent.owner));
        EXPECT_FALSE(allowed(manager, Flag::Build, child.owner));
    }
    parent.flags[Flag::Build] = FlagState::Deny;
    manager.replaceAll({parent, child});
    EXPECT_FALSE(allowed(manager, Flag::Build, parent.owner));
    child.flags[Flag::Build] = FlagState::Allow;
    manager.replaceAll({parent, child});
    EXPECT_TRUE(allowed(manager));
}

TEST(RegionPassthrough, ParentGeometryIsNotInventedOutsideItsBounds)
{
    auto parent = region("parent");
    auto child = region("child");
    child.bounds = {{90, -10, -10}, {110, 10, 10}};
    child.parent = parent.key.name;
    child.passthrough = FlagState::Allow;
    RegionManager manager;
    manager.replaceAll({parent, child});
    EXPECT_TRUE(manager.isAllowed(dimension, outside, Flag::Build, "outsider"));
    EXPECT_FALSE(allowed(manager));
    parent.flags[Flag::Build] = FlagState::Deny;
    manager.replaceAll({parent, child});
    EXPECT_FALSE(manager.isAllowed(dimension, outside, Flag::Build, "outsider"));
}

TEST(RegionGroups, InheritedOwnersDoNotIncludeAncestorsPlainMembers)
{
    auto grandparent = region("grandparent", RegionKind::Template);
    grandparent.flags[Flag::Pvp] = FlagState::Deny;
    grandparent.flag_groups[Flag::Pvp] = RegionGroup::Owners;
    grandparent.members.insert("grandparent-member");
    auto parent = region("parent", RegionKind::Template);
    parent.parent = grandparent.key.name;
    auto child = region("child");
    child.parent = parent.key.name;
    RegionManager manager;
    manager.replaceAll({grandparent, parent, child});
    EXPECT_FALSE(allowed(manager, Flag::Pvp, grandparent.owner));
    EXPECT_FALSE(allowed(manager, Flag::Pvp, parent.owner));
    EXPECT_FALSE(allowed(manager, Flag::Pvp, child.owner));
    EXPECT_TRUE(allowed(manager, Flag::Pvp, "grandparent-member"));
    EXPECT_TRUE(allowed(manager, Flag::Pvp));
}

TEST(RegionHierarchy, EnvironmentalInheritanceRemainsSeparateFromMembershipAndBypass)
{
    auto parent = region("parent", RegionKind::Template);
    parent.flags[Flag::Explosions] = FlagState::Deny;
    parent.flags[Flag::Invincible] = FlagState::Allow;
    auto child = region("child");
    child.parent = parent.key.name;
    child.flags[Flag::Build] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({parent, child});
    EXPECT_FALSE(manager.isEnvironmentAllowed(dimension, inside, Flag::Explosions));
    EXPECT_TRUE(manager.isEnvironmentAllowed(dimension, inside, Flag::Invincible));
    EXPECT_FALSE(allowed(manager, Flag::Build, child.owner));
    EXPECT_TRUE(manager.isAllowed(dimension, inside, Flag::Build, "outsider", true));
    EXPECT_THROW(static_cast<void>(manager.isEnvironmentAllowed(dimension, inside, Flag::Build)),
                 std::invalid_argument);
}

TEST(RegionHierarchy, TransitionGroupsUseInheritedMembershipAndChangedRegionsOnly)
{
    auto parent = region("parent", RegionKind::Template);
    parent.members.insert("inherited-member");
    parent.flags[Flag::Entry] = FlagState::Deny;
    parent.flags[Flag::Exit] = FlagState::Deny;
    parent.flag_groups[Flag::Entry] = RegionGroup::NonMembers;
    parent.flag_groups[Flag::Exit] = RegionGroup::NonMembers;
    auto child = region("child");
    child.parent = parent.key.name;
    child.passthrough = FlagState::Allow;
    RegionManager manager;
    manager.replaceAll({parent, child});
    EXPECT_FALSE(manager.isTransitionAllowed(dimension, outside, dimension, inside, "outsider"));
    EXPECT_FALSE(manager.isTransitionAllowed(dimension, inside, dimension, outside, "outsider"));
    EXPECT_TRUE(manager.isTransitionAllowed(dimension, outside, dimension, inside, "inherited-member"));
    EXPECT_TRUE(manager.isTransitionAllowed(dimension, inside, dimension, outside, child.owner));
    EXPECT_TRUE(manager.isTransitionAllowed(dimension, inside, dimension, {1, 0, 0}, "outsider"));
    EXPECT_TRUE(manager.isTransitionAllowed(dimension, outside, dimension, inside, "outsider", true));
}

TEST(RegionHierarchy, GlobalTransitionFlagsOnlyApplyToDimensionCrossings)
{
    auto global = region("unused", RegionKind::Global);
    global.flags[Flag::Entry] = FlagState::Deny;
    global.flags[Flag::Exit] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({global});
    EXPECT_TRUE(manager.isTransitionAllowed(dimension, inside, dimension, outside, "outsider"));
    EXPECT_FALSE(manager.isTransitionAllowed(other_dimension, inside, dimension, inside, "outsider"));
    EXPECT_FALSE(manager.isTransitionAllowed(dimension, inside, other_dimension, inside, "outsider"));
}

TEST(RegionHierarchy, CopyMoveAndFailedReplacementRetainTheirOwnSnapshots)
{
    auto parent = region("parent", RegionKind::Template);
    parent.flags[Flag::Build] = FlagState::Allow;
    auto child = region("child");
    child.parent = parent.key.name;
    auto global = region("unused", RegionKind::Global);
    global.flags[Flag::Pvp] = FlagState::Deny;
    RegionManager original;
    original.replaceAll({parent, child, global});
    RegionManager copied(original);
    RegionManager assigned;
    assigned = original;
    original.replaceAll({});
    EXPECT_TRUE(allowed(copied));
    EXPECT_TRUE(allowed(assigned));
    EXPECT_FALSE(allowed(copied, Flag::Pvp));
    RegionManager moved(std::move(copied));
    EXPECT_TRUE(allowed(moved));
    EXPECT_TRUE(copied.getAll().empty());
    RegionManager move_assigned;
    move_assigned = std::move(assigned);
    EXPECT_TRUE(allowed(move_assigned));
    EXPECT_TRUE(assigned.getAll().empty());
    const auto *before = moved.find(child.key);
    child.parent = "missing";
    EXPECT_THROW(moved.replaceAll({child}), std::invalid_argument);
    EXPECT_EQ(moved.find(before->key), before);
    EXPECT_TRUE(allowed(moved));
}

TEST(RegionHierarchy, DirectPoliciesRetainDefaultContextCompatibility)
{
    auto value = region();
    value.flags[Flag::Pvp] = FlagState::Deny;
    const std::array<const Region *, 1> matching{&value};
    EXPECT_FALSE(ProtectionPolicy::isAllowed(matching, Flag::Pvp, value.owner));
    EXPECT_TRUE(ProtectionPolicy::isAllowed(matching, Flag::Build, value.owner));
    EXPECT_FALSE(ProtectionPolicy::isAllowed(matching, Flag::Build, "outsider"));
    RegionContext context;
    EXPECT_FALSE(context.isOwner(Region{}, ""));
    EXPECT_FALSE(context.isMember(Region{}, ""));
    value.parent = "unresolved";
    EXPECT_THROW(static_cast<void>(ProtectionPolicy::isAllowed(matching, Flag::Pvp, value.owner)),
                 std::invalid_argument);
}

TEST(RegionHierarchy, CompiledContextRejectsMismatchedSnapshotsAndParentIndices)
{
    auto parent = region("parent", RegionKind::Template);
    auto child = region("child");
    child.parent = parent.key.name;
    const std::array regions{parent, child};
    const std::array<std::optional<std::size_t>, 2> parents{std::nullopt, 0};
    const RegionContext context{regions, parents};
    EXPECT_EQ(context.parent(regions[1]), &regions[0]);
    EXPECT_EQ(context.parent(regions[0]), nullptr);
    EXPECT_THROW(static_cast<void>(context.parent(child)), std::invalid_argument);
    EXPECT_THROW((RegionContext{regions, {}}), std::invalid_argument);
    const std::array<std::optional<std::size_t>, 2> invalid_parents{std::nullopt, regions.size()};
    const RegionContext invalid{regions, invalid_parents};
    EXPECT_THROW(static_cast<void>(invalid.parent(regions[1])), std::invalid_argument);
    const std::array<std::optional<std::size_t>, 2> missing_parents{std::nullopt, std::nullopt};
    const RegionContext missing{regions, missing_parents};
    EXPECT_THROW(static_cast<void>(missing.parent(regions[1])), std::invalid_argument);
}

}
}
