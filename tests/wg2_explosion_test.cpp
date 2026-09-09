#include "dimenguard/region/region_manager.h"
#include "dimenguard/rules/explosion_rules.h"

#include <array>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace dimenguard {
namespace {

const DimensionKey overworld{"world", "minecraft:overworld"};
const DimensionKey nether{"world", "minecraft:nether"};
constexpr ExplosionActorType tnt{"minecraft", "tnt"};
constexpr ExplosionActorType creeper{"minecraft", "creeper"};
constexpr ExplosionActorType player{"minecraft", "player"};

Region protectedRegion()
{
    Region region;
    region.key = {overworld, "plot"};
    region.bounds = {{0, -64, 0}, {15, 100, 15}};
    region.owner = "owner";
    return region;
}

void filterAt(const RegionManager &manager, ExplosionSource source, std::optional<BlockPosition> origin,
              std::vector<BlockPosition> &blocks, const DimensionKey &target_dimension = overworld,
              const DimensionKey &source_dimension = overworld)
{
    filterExplosionBlocks(
        blocks, source,
        [&](const auto &block, Flag flag) { return manager.isEnvironmentAllowed(target_dimension, block, flag); },
        [&](const auto &block) {
            if (source != ExplosionSource::Tnt && source != ExplosionSource::Unknown) {
                return true;
            }
            return manager.isNonPlayerAllowed(source_dimension, source == ExplosionSource::Tnt ? origin : std::nullopt,
                                              target_dimension, block, Flag::BlockBreak);
        });
}

TEST(ExplosionSources, RecognizesOnlyExactVanillaTntAndCreeperIdentifiers)
{
    EXPECT_EQ(explosionSource(tnt), ExplosionSource::Tnt);
    EXPECT_EQ(explosionSource({"minecraft", "tnt_minecart"}), ExplosionSource::Tnt);
    EXPECT_EQ(explosionSource(creeper), ExplosionSource::Creeper);
    for (const auto key : {"TNT", "tnt_extra", "primed_tnt", "creeper_extra", "wither", "fireball"}) {
        EXPECT_EQ(explosionSource({"minecraft", key}), ExplosionSource::Other) << key;
    }
    for (const auto actor_namespace : {"custom", "Minecraft"}) {
        EXPECT_EQ(explosionSource({actor_namespace, "tnt"}), ExplosionSource::Other);
        EXPECT_EQ(explosionSource({actor_namespace, "creeper"}), ExplosionSource::Other);
    }
}

TEST(ExplosionSources, EmptyIdentifiersRemainUnknown)
{
    EXPECT_EQ(explosionSource({"", "tnt"}), ExplosionSource::Unknown);
    EXPECT_EQ(explosionSource({"minecraft", ""}), ExplosionSource::Unknown);
    EXPECT_EQ(explosionSource({"", ""}), ExplosionSource::Unknown);
}

TEST(ExplosionSources, IgnoresNonExplosionDamageEvenWithRecognizedActors)
{
    for (const auto cause : {"entity_attack", "projectile", "fireworks", "self_destruct", "explosion", ""}) {
        EXPECT_EQ(explosionDamageSource(cause, tnt, creeper), std::nullopt) << cause;
    }
}

TEST(ExplosionSources, DirectActorTakesPrecedenceOverCreditedActor)
{
    EXPECT_EQ(explosionDamageSource("entity_explosion", tnt, player), ExplosionSource::Tnt);
    EXPECT_EQ(explosionDamageSource("block_explosion", creeper, tnt), ExplosionSource::Creeper);
    EXPECT_EQ(explosionDamageSource("entity_explosion", ExplosionActorType{"custom", "bomb"}, tnt),
              ExplosionSource::Other);
}

TEST(ExplosionSources, MissingDirectActorUsesOnlyRecognizedExplosiveCredits)
{
    EXPECT_EQ(explosionDamageSource("entity_explosion", std::nullopt, tnt), ExplosionSource::Tnt);
    EXPECT_EQ(explosionDamageSource("entity_explosion", std::nullopt, creeper), ExplosionSource::Creeper);
    EXPECT_EQ(explosionDamageSource("entity_explosion", std::nullopt, player), ExplosionSource::Unknown);
    EXPECT_EQ(explosionDamageSource("entity_explosion", std::nullopt, ExplosionActorType{"custom", "bomb"}),
              ExplosionSource::Unknown);
    EXPECT_EQ(explosionDamageSource("block_explosion"), ExplosionSource::Unknown);
    EXPECT_EQ(explosionDamageSource("entity_explosion"), ExplosionSource::Unknown);
}

TEST(ExplosionSources, UnidentifiableDirectActorCannotBeReplacedByCreditedActor)
{
    EXPECT_EQ(explosionDamageSource("entity_explosion", ExplosionActorType{"", "tnt"}, tnt), ExplosionSource::Unknown);
    EXPECT_EQ(explosionDamageSource("block_explosion", ExplosionActorType{"minecraft", ""}, creeper),
              ExplosionSource::Unknown);
}

TEST(ExplosionSources, UnknownSourcesMustPassEverySubtype)
{
    for (const auto denied : {Flag::Tnt, Flag::CreeperExplosion, Flag::OtherExplosion}) {
        EXPECT_FALSE(allowsExplosion(ExplosionSource::Unknown, [denied](Flag flag) { return flag != denied; }));
    }
    EXPECT_TRUE(allowsExplosion(ExplosionSource::Unknown, [](Flag) { return true; }));
    EXPECT_FALSE(allowsExplosion(static_cast<ExplosionSource>(99), [](Flag flag) { return flag != Flag::Tnt; }));
}

TEST(ExplosionSources, KnownSourcesQueryOnlyTheirSubtype)
{
    const std::array cases{std::pair{ExplosionSource::Tnt, Flag::Tnt},
                           std::pair{ExplosionSource::Creeper, Flag::CreeperExplosion},
                           std::pair{ExplosionSource::Other, Flag::OtherExplosion}};
    for (const auto &[source, expected] : cases) {
        std::vector<Flag> queried;
        EXPECT_TRUE(allowsExplosion(source, [&](Flag flag) {
            queried.push_back(flag);
            return true;
        }));
        EXPECT_EQ(queried, std::vector{expected});
    }
}

TEST(ExplosionFiltering, PreservesAllowedOrderAndDuplicates)
{
    std::vector blocks{1, 2, 3, 2, 4, 5};
    filterExplosionBlocks(
        blocks, ExplosionSource::Tnt, [](int block, Flag) { return block % 2 == 0; }, [](int) { return true; });
    EXPECT_EQ(blocks, (std::vector{2, 2, 4}));
}

TEST(ExplosionFiltering, ChecksBoundaryOnceAndSkipsStateQueriesOnDeniedBlocks)
{
    std::vector blocks{1, 2, 3};
    int boundaries = 0;
    int states = 0;
    filterExplosionBlocks(
        blocks, ExplosionSource::Unknown,
        [&](int, Flag) {
            ++states;
            return true;
        },
        [&](int block) {
            ++boundaries;
            return block == 2;
        });
    EXPECT_EQ(blocks, (std::vector{2}));
    EXPECT_EQ(boundaries, 3);
    EXPECT_EQ(states, 3);
}

TEST(ExplosionFiltering, EmptyListsPerformNoQueries)
{
    std::vector<int> blocks;
    int queries = 0;
    filterExplosionBlocks(
        blocks, ExplosionSource::Unknown,
        [&](int, Flag) {
            ++queries;
            return false;
        },
        [&](int) {
            ++queries;
            return false;
        });
    EXPECT_TRUE(blocks.empty());
    EXPECT_EQ(queries, 0);
}

TEST(ExplosionBoundaries, OutsideTntKeepsUnprotectedTerrainAndFiltersProtectedBoundary)
{
    RegionManager manager;
    manager.replaceAll({protectedRegion()});
    std::vector<BlockPosition> blocks{{16, 64, 0}, {15, 64, 0}, {14, 64, 0}, {17, 64, 0}};
    filterAt(manager, ExplosionSource::Tnt, BlockPosition{16, 64, 0}, blocks);
    EXPECT_EQ(blocks, (std::vector<BlockPosition>{{16, 64, 0}, {17, 64, 0}}));
}

TEST(ExplosionBoundaries, TntInsideRegionHasNonPlayerMembershipWithoutPlayerUuid)
{
    RegionManager manager;
    manager.replaceAll({protectedRegion()});
    std::vector<BlockPosition> blocks{{14, 64, 0}, {15, 64, 0}, {16, 64, 0}};
    const auto expected = blocks;
    filterAt(manager, ExplosionSource::Tnt, BlockPosition{14, 64, 0}, blocks);
    EXPECT_EQ(blocks, expected);
}

TEST(ExplosionBoundaries, TntAllowDoesNotGrantOutsideSourceTerrainMembership)
{
    auto region = protectedRegion();
    region.flags[Flag::Tnt] = FlagState::Allow;
    RegionManager manager;
    manager.replaceAll({region});
    std::vector<BlockPosition> blocks{{15, 64, 0}, {16, 64, 0}};
    filterAt(manager, ExplosionSource::Tnt, BlockPosition{16, 64, 0}, blocks);
    EXPECT_EQ(blocks, (std::vector<BlockPosition>{{16, 64, 0}}));
}

TEST(ExplosionBoundaries, SameRegionMembershipDoesNotOverrideExplicitBlockBreakDeny)
{
    auto region = protectedRegion();
    region.flags[Flag::BlockBreak] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({region});
    std::vector<BlockPosition> blocks{{14, 64, 0}, {15, 64, 0}, {16, 64, 0}};
    filterAt(manager, ExplosionSource::Tnt, BlockPosition{14, 64, 0}, blocks);
    EXPECT_EQ(blocks, (std::vector<BlockPosition>{{16, 64, 0}}));
}

TEST(ExplosionBoundaries, NegativeInclusiveBoundaryRemainsProtected)
{
    auto region = protectedRegion();
    region.bounds = {{-16, -64, -16}, {-1, 100, -1}};
    RegionManager manager;
    manager.replaceAll({region});
    std::vector<BlockPosition> blocks{{-17, 64, -1}, {-16, 64, -1}, {-1, 64, -1}, {0, 64, -1}};
    filterAt(manager, ExplosionSource::Tnt, BlockPosition{-17, 64, -1}, blocks);
    EXPECT_EQ(blocks, (std::vector<BlockPosition>{{-17, 64, -1}, {0, 64, -1}}));
}

TEST(ExplosionBoundaries, UnknownSourcesDoNotAcquireMembershipFromReportedOrigin)
{
    RegionManager manager;
    manager.replaceAll({protectedRegion()});
    std::vector<BlockPosition> blocks{{14, 64, 0}, {16, 64, 0}};
    filterAt(manager, ExplosionSource::Unknown, BlockPosition{14, 64, 0}, blocks);
    EXPECT_EQ(blocks, (std::vector<BlockPosition>{{16, 64, 0}}));
}

TEST(ExplosionBoundaries, ExistingAggregateStillFiltersAllSourceTypes)
{
    auto region = protectedRegion();
    region.flags[Flag::Explosions] = FlagState::Deny;
    region.flags[Flag::Build] = FlagState::Allow;
    RegionManager manager;
    manager.replaceAll({region});
    for (const auto source :
         {ExplosionSource::Tnt, ExplosionSource::Creeper, ExplosionSource::Other, ExplosionSource::Unknown}) {
        std::vector<BlockPosition> blocks{{15, 64, 0}, {16, 64, 0}};
        filterAt(manager, source, BlockPosition{16, 64, 0}, blocks);
        EXPECT_EQ(blocks, (std::vector<BlockPosition>{{16, 64, 0}}));
    }
}

TEST(ExplosionBoundaries, ExplicitSubtypeOverridesInheritedAggregateForTerrainAndDamage)
{
    auto parent = protectedRegion();
    parent.key.name = "parent";
    parent.kind = RegionKind::Template;
    parent.flags[Flag::Explosions] = FlagState::Deny;
    auto region = protectedRegion();
    region.parent = parent.key.name;
    region.flags[Flag::Tnt] = FlagState::Allow;
    region.flags[Flag::Build] = FlagState::Allow;
    RegionManager manager;
    manager.replaceAll({parent, region});

    std::vector<BlockPosition> blocks{{15, 64, 0}, {16, 64, 0}};
    const auto expected = blocks;
    filterAt(manager, ExplosionSource::Tnt, BlockPosition{16, 64, 0}, blocks);
    EXPECT_EQ(blocks, expected);
    EXPECT_TRUE(allowsExplosion(ExplosionSource::Tnt,
                                [&](Flag flag) { return manager.isEnvironmentAllowed(overworld, {15, 64, 0}, flag); }));
    EXPECT_FALSE(allowsExplosion(ExplosionSource::Creeper, [&](Flag flag) {
        return manager.isEnvironmentAllowed(overworld, {15, 64, 0}, flag);
    }));
    EXPECT_FALSE(allowsExplosion(ExplosionSource::Unknown, [&](Flag flag) {
        return manager.isEnvironmentAllowed(overworld, {15, 64, 0}, flag);
    }));
}

TEST(ExplosionBoundaries, InheritedSubtypeDenyOverridesLocalAggregateAllow)
{
    auto parent = protectedRegion();
    parent.key.name = "parent";
    parent.kind = RegionKind::Template;
    parent.flags[Flag::Tnt] = FlagState::Deny;
    auto region = protectedRegion();
    region.parent = parent.key.name;
    region.flags[Flag::Explosions] = FlagState::Allow;
    region.flags[Flag::Build] = FlagState::Allow;
    RegionManager manager;
    manager.replaceAll({parent, region});

    std::vector<BlockPosition> blocks{{15, 64, 0}, {16, 64, 0}};
    filterAt(manager, ExplosionSource::Tnt, BlockPosition{16, 64, 0}, blocks);
    EXPECT_EQ(blocks, (std::vector<BlockPosition>{{16, 64, 0}}));
}

TEST(ExplosionBoundaries, UnrelatedEqualPrioritySubtypeDenyWins)
{
    auto first = protectedRegion();
    first.flags[Flag::Tnt] = FlagState::Allow;
    first.flags[Flag::Build] = FlagState::Allow;
    auto second = first;
    second.key.name = "other";
    second.flags[Flag::Tnt] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({first, second});

    std::vector<BlockPosition> blocks{{15, 64, 0}, {16, 64, 0}};
    filterAt(manager, ExplosionSource::Tnt, BlockPosition{16, 64, 0}, blocks);
    EXPECT_EQ(blocks, (std::vector<BlockPosition>{{16, 64, 0}}));
}

TEST(ExplosionBoundaries, UnknownDamageCannotUseOtherAllowToBypassTntDeny)
{
    auto region = protectedRegion();
    region.flags[Flag::Tnt] = FlagState::Deny;
    region.flags[Flag::OtherExplosion] = FlagState::Allow;
    region.flags[Flag::Build] = FlagState::Allow;
    RegionManager manager;
    manager.replaceAll({region});
    std::vector<BlockPosition> blocks{{15, 64, 0}, {16, 64, 0}};
    filterAt(manager, ExplosionSource::Unknown, std::nullopt, blocks);
    EXPECT_EQ(blocks, (std::vector<BlockPosition>{{16, 64, 0}}));
}

TEST(ExplosionBoundaries, IdentifiedCreeperAndOtherKeepEnvironmentDefaults)
{
    auto region = protectedRegion();
    region.flags[Flag::Build] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({region});
    for (const auto source : {ExplosionSource::Creeper, ExplosionSource::Other}) {
        std::vector<BlockPosition> blocks{{15, 64, 0}, {16, 64, 0}};
        const auto expected = blocks;
        filterAt(manager, source, BlockPosition{16, 64, 0}, blocks);
        EXPECT_EQ(blocks, expected);
    }
}

TEST(ExplosionBoundaries, StateFilteringDoesNotLeakAcrossDimensions)
{
    auto region = protectedRegion();
    region.flags[Flag::Explosions] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({region});
    std::vector<BlockPosition> blocks{{15, 64, 0}, {16, 64, 0}};
    const auto expected = blocks;
    filterAt(manager, ExplosionSource::Other, std::nullopt, blocks, nether);
    EXPECT_EQ(blocks, expected);
}

TEST(ExplosionBoundaries, KnownTntRejectsCrossDimensionAffectedBlocks)
{
    auto region = protectedRegion();
    region.key.dimension = nether;
    region.flags[Flag::Build] = FlagState::Allow;
    RegionManager manager;
    manager.replaceAll({region});
    std::vector<BlockPosition> blocks{{15, 64, 0}, {16, 64, 0}};
    filterAt(manager, ExplosionSource::Tnt, BlockPosition{15, 64, 0}, blocks, nether, overworld);
    EXPECT_TRUE(blocks.empty());
}

TEST(ExplosionBoundaries, SameCoordinatesInAnotherDimensionDoNotGiveTntMembership)
{
    auto region = protectedRegion();
    region.key.dimension = nether;
    RegionManager manager;
    manager.replaceAll({region});
    std::vector<BlockPosition> blocks{{15, 64, 0}, {16, 64, 0}};
    const auto expected = blocks;
    filterAt(manager, ExplosionSource::Tnt, BlockPosition{16, 64, 0}, blocks, overworld, overworld);
    EXPECT_EQ(blocks, expected);
    filterAt(manager, ExplosionSource::Tnt, BlockPosition{16, 64, 0}, blocks, nether, nether);
    EXPECT_EQ(blocks, (std::vector<BlockPosition>{{16, 64, 0}}));
}

TEST(ExplosionBoundaries, TerrainMembershipIsNotVictimDamageImmunity)
{
    RegionManager manager;
    manager.replaceAll({protectedRegion()});
    std::vector<BlockPosition> blocks{{15, 64, 0}};
    filterAt(manager, ExplosionSource::Tnt, BlockPosition{16, 64, 0}, blocks);
    EXPECT_TRUE(blocks.empty());
    EXPECT_TRUE(allowsExplosion(ExplosionSource::Tnt,
                                [&](Flag flag) { return manager.isEnvironmentAllowed(overworld, {15, 64, 0}, flag); }));
}

}
}
