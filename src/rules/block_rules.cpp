#include "dimenguard/rules/block_rules.h"

#include <algorithm>
#include <array>
#include <span>

namespace dimenguard {
namespace {

using namespace std::string_view_literals;

constexpr std::array wood_types{"acacia"sv, "bamboo"sv,   "birch"sv,    "cherry"sv, "crimson"sv, "dark_oak"sv,
                                "jungle"sv, "mangrove"sv, "pale_oak"sv, "spruce"sv, "warped"sv};
constexpr std::array wood_actions{"_door"sv, "_trapdoor"sv, "_fence_gate"sv, "_button"sv};
constexpr std::array copper_types{
    "copper"sv,       "exposed_copper"sv,       "weathered_copper"sv,       "oxidized_copper"sv,
    "waxed_copper"sv, "waxed_exposed_copper"sv, "waxed_weathered_copper"sv, "waxed_oxidized_copper"sv};
constexpr std::array copper_actions{"_door"sv, "_trapdoor"sv};
constexpr std::array other_use_blocks{
    "wooden_door"sv, "trapdoor"sv,      "fence_gate"sv,   "wooden_button"sv,
    "iron_door"sv,   "iron_trapdoor"sv, "stone_button"sv, "polished_blackstone_button"sv,
    "lever"sv};
constexpr std::array anvils{"anvil"sv, "chipped_anvil"sv, "damaged_anvil"sv, "deprecated_anvil"sv};

bool contains(std::span<const std::string_view> values, std::string_view value)
{
    return std::ranges::find(values, value) != values.end();
}

bool matchesVariant(std::string_view key, std::span<const std::string_view> types,
                    std::span<const std::string_view> actions)
{
    return std::ranges::any_of(actions, [key, types](const auto action) {
        return key.ends_with(action) && contains(types, key.substr(0, key.size() - action.size()));
    });
}

}

std::optional<Flag> blockUseFlag(std::string_view block_namespace, std::string_view block_key)
{
    // A suffix alone does not establish the behavior of a custom block.
    if (block_namespace != "minecraft") {
        return std::nullopt;
    }
    if (contains(anvils, block_key)) {
        return Flag::UseAnvil;
    }
    if (contains(other_use_blocks, block_key) || matchesVariant(block_key, wood_types, wood_actions) ||
        matchesVariant(block_key, copper_types, copper_actions)) {
        return Flag::Use;
    }
    return std::nullopt;
}

Flag fluidFlowFlag(std::string_view block_namespace, std::string_view block_key)
{
    if (block_namespace == "minecraft") {
        if (block_key == "water" || block_key == "flowing_water") {
            return Flag::WaterFlow;
        }
        if (block_key == "lava" || block_key == "flowing_lava") {
            return Flag::LavaFlow;
        }
    }
    return Flag::FluidFlow;
}

}
