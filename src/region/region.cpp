#include "dimenguard/region/region.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace dimenguard {
namespace {

constexpr std::array flag_names = {
    std::pair{Flag::Build, std::string_view{"build"}},
    std::pair{Flag::Interact, std::string_view{"interact"}},
    std::pair{Flag::ContainerAccess, std::string_view{"container-access"}},
    std::pair{Flag::Pvp, std::string_view{"pvp"}},
};

constexpr std::array state_names = {
    std::pair{FlagState::Inherit, std::string_view{"inherit"}},
    std::pair{FlagState::Allow, std::string_view{"allow"}},
    std::pair{FlagState::Deny, std::string_view{"deny"}},
};

template <typename Enum, std::size_t Size>
std::string_view findName(const std::array<std::pair<Enum, std::string_view>, Size> &names, Enum value)
{
    for (const auto &[candidate, name] : names) {
        if (candidate == value) {
            return name;
        }
    }
    throw std::invalid_argument("Unknown region flag or state");
}

template <typename Enum, std::size_t Size>
std::optional<Enum> findValue(const std::array<std::pair<Enum, std::string_view>, Size> &names, std::string_view name)
{
    for (const auto &[value, candidate] : names) {
        if (candidate == name) {
            return value;
        }
    }
    return std::nullopt;
}

}  // namespace

Bounds Bounds::between(const BlockPosition &a, const BlockPosition &b)
{
    return {{std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)},
            {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}};
}

bool Bounds::contains(const BlockPosition &position) const
{
    return position.x >= min.x && position.x <= max.x && position.y >= min.y && position.y <= max.y &&
           position.z >= min.z && position.z <= max.z;
}

std::string_view flagName(Flag flag)
{
    return findName(flag_names, flag);
}

std::optional<Flag> parseFlag(std::string_view name)
{
    return findValue(flag_names, name);
}

std::string_view stateName(FlagState state)
{
    return findName(state_names, state);
}

std::optional<FlagState> parseState(std::string_view name)
{
    return findValue(state_names, name);
}

bool Region::isMember(std::string_view player_id) const
{
    return player_id == owner || members.contains(std::string(player_id));
}

bool isValidRegionName(std::string_view name)
{
    return !name.empty() && name.size() <= 64 && std::ranges::all_of(name, [](char character) {
        return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '_' ||
               character == '-';
    });
}

void validateRegion(const Region &region)
{
    if (region.key.dimension.level.empty() || region.key.dimension.dimension.empty() || region.owner.empty()) {
        throw std::invalid_argument("Region level, dimension and owner must not be empty");
    }
    if (!isValidRegionName(region.key.name)) {
        throw std::invalid_argument("Region names must contain 1-64 lowercase letters, digits, underscores or hyphens");
    }
    const auto &bounds = region.bounds;
    if (bounds.min.x > bounds.max.x || bounds.min.y > bounds.max.y || bounds.min.z > bounds.max.z) {
        throw std::invalid_argument("Region bounds must be ordered on every axis");
    }
    if (region.members.contains("")) {
        throw std::invalid_argument("Region member identities must not be empty");
    }
    for (const auto &[flag, state] : region.flags) {
        static_cast<void>(flagName(flag));
        static_cast<void>(stateName(state));
    }
}

}  // namespace dimenguard
