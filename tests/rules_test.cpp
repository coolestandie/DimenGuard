#include "dimenguard/rules/block_rules.h"
#include "dimenguard/rules/damage_rules.h"

#include <array>
#include <gtest/gtest.h>
#include <optional>
#include <string>

namespace dimenguard {
namespace {

TEST(BlockUseRules, ClassifiesReviewedWoodVariants)
{
    constexpr std::array types{"acacia", "bamboo",   "birch",    "cherry", "crimson", "dark_oak",
                               "jungle", "mangrove", "pale_oak", "spruce", "warped"};
    constexpr std::array actions{"_door", "_trapdoor", "_fence_gate", "_button"};
    for (const auto type : types) {
        for (const auto action : actions) {
            const auto key = std::string(type) + action;
            EXPECT_EQ(blockUseFlag("minecraft", key), Flag::Use) << key;
        }
    }
}

TEST(BlockUseRules, ClassifiesLegacyOakIronAndSwitchIdentifiers)
{
    constexpr std::array keys{"wooden_door", "trapdoor",      "fence_gate",   "wooden_button",
                              "iron_door",   "iron_trapdoor", "stone_button", "polished_blackstone_button",
                              "lever"};
    for (const auto key : keys) {
        EXPECT_EQ(blockUseFlag("minecraft", key), Flag::Use) << key;
    }
}

TEST(BlockUseRules, ClassifiesAllCopperDoorAndTrapdoorStages)
{
    constexpr std::array types{
        "copper",       "exposed_copper",       "weathered_copper",       "oxidized_copper",
        "waxed_copper", "waxed_exposed_copper", "waxed_weathered_copper", "waxed_oxidized_copper"};
    for (const auto type : types) {
        for (const auto action : {"_door", "_trapdoor"}) {
            const auto key = std::string(type) + action;
            EXPECT_EQ(blockUseFlag("minecraft", key), Flag::Use) << key;
        }
    }
}

TEST(BlockUseRules, KeepsAnvilsOnTheirSpecificRule)
{
    for (const auto key : {"anvil", "chipped_anvil", "damaged_anvil", "deprecated_anvil"}) {
        EXPECT_EQ(blockUseFlag("minecraft", key), Flag::UseAnvil) << key;
    }
}

TEST(BlockUseRules, LeavesUnreviewedBlocksToExistingInteractionPolicy)
{
    constexpr std::array keys{"",
                              "stone",
                              "chest",
                              "trapped_chest",
                              "barrel",
                              "bed",
                              "respawn_anchor",
                              "oak_pressure_plate",
                              "stone_pressure_plate",
                              "light_weighted_pressure_plate",
                              "acacia_fence",
                              "acacia_door_extra",
                              "fake_acacia_door",
                              "copper_button",
                              "copper_fence_gate",
                              "gold_door",
                              "door",
                              "_door",
                              "anvil_extra",
                              "gold_anvil",
                              "oak_door",
                              "minecraft:wooden_door",
                              "WOODEN_DOOR"};
    for (const auto key : keys) {
        EXPECT_EQ(blockUseFlag("minecraft", key), std::nullopt) << key;
    }
}

TEST(BlockUseRules, DoesNotAssignVanillaBehaviorToCustomNamespaces)
{
    for (const auto block_namespace : {"", "custom", "Minecraft", "minecraft:custom"}) {
        for (const auto key : {"wooden_door", "pale_oak_trapdoor", "copper_door", "lever", "anvil"}) {
            EXPECT_EQ(blockUseFlag(block_namespace, key), std::nullopt) << block_namespace << ':' << key;
        }
    }
}

TEST(FluidFlowRules, ClassifiesStillAndFlowingVariants)
{
    EXPECT_EQ(fluidFlowFlag("minecraft", "water"), Flag::WaterFlow);
    EXPECT_EQ(fluidFlowFlag("minecraft", "flowing_water"), Flag::WaterFlow);
    EXPECT_EQ(fluidFlowFlag("minecraft", "lava"), Flag::LavaFlow);
    EXPECT_EQ(fluidFlowFlag("minecraft", "flowing_lava"), Flag::LavaFlow);
}

TEST(FluidFlowRules, KeepsUnknownTypesOnTheAggregateRule)
{
    for (const auto key : {"", "air", "lava_cauldron", "still_water", "waterlogged", "waterlily", "minecraft:lava",
                           "WATER", "flowing_water_extra"}) {
        EXPECT_EQ(fluidFlowFlag("minecraft", key), Flag::FluidFlow) << key;
    }
    for (const auto block_namespace : {"", "custom", "Minecraft"}) {
        for (const auto key : {"water", "flowing_water", "lava", "flowing_lava"}) {
            EXPECT_EQ(fluidFlowFlag(block_namespace, key), Flag::FluidFlow) << block_namespace << ':' << key;
        }
    }
}

TEST(DamageRules, MapsTheReportedFallAndFireworkCauseNames)
{
    EXPECT_EQ(damageFlag("fall"), Flag::FallDamage);
    EXPECT_EQ(damageFlag("fireworks"), Flag::FireworkDamage);
}

TEST(DamageRules, DoesNotConflateRelatedOrUnknownCauses)
{
    for (const auto type : {"", "none", "falling_block", "fly_into_wall", "stalactite", "stalagmite", "anvil",
                            "projectile", "block_explosion", "entity_explosion", "entity_attack", "firework", "fire",
                            "fire_tick", "minecraft:fall", "FALL"}) {
        EXPECT_EQ(damageFlag(type), std::nullopt) << type;
    }
}

}
}
