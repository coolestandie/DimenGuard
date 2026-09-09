#include "dimenguard/protection/protection_context.h"

#include "dimenguard/adapter/context.h"
#include "dimenguard/plugin.h"
#include "dimenguard/region/coordinates.h"

#include <array>
#include <stdexcept>

namespace dimenguard {
namespace {

constexpr auto error_interval = std::chrono::seconds(10);

DimensionKey targetDimension(const endstone::Location &location)
{
    const auto dimension = location.getDimension();
    if (!dimension) {
        throw std::invalid_argument("Protection target has no dimension");
    }
    return dimensionKey(*dimension);
}

}

ProtectionContext::ProtectionContext(DimenGuardPlugin &plugin) : plugin_(plugin) {}

endstone::Plugin &ProtectionContext::getPlugin()
{
    return plugin_;
}

bool ProtectionContext::allowed(endstone::Player &player, const endstone::Location &location, Flag flag)
{
    const auto *service = plugin_.getService();
    if (!service) {
        plugin_.getMessenger().deny(player, Message::NotReady);
        return false;
    }
    if (service->getRegions().isAllowed(targetDimension(location), blockPosition(location), flag,
                                        player.getUniqueId().str(), player.hasPermission("dimenguard.bypass"))) {
        return true;
    }
    plugin_.getMessenger().deny(player);
    return false;
}

bool ProtectionContext::allowed(const endstone::Location &location, Flag flag)
{
    const auto *service = plugin_.getService();
    if (!service) {
        return false;
    }
    return service->getRegions().isEnvironmentAllowed(targetDimension(location), blockPosition(location), flag);
}

bool ProtectionContext::permitsDamage(const endstone::Location &location, bool player, std::optional<Flag> damage_flag)
{
    const auto *service = plugin_.getService();
    if (!service) {
        return false;
    }
    const auto dimension = targetDimension(location);
    const auto position = blockPosition(location);
    const auto &regions = service->getRegions();
    if (player && regions.isEnvironmentAllowed(dimension, position, Flag::Invincible)) {
        return false;
    }
    return !damage_flag || regions.isEnvironmentAllowed(dimension, position, *damage_flag);
}

bool ProtectionContext::allowedAtBoth(endstone::Player &player, const endstone::Location &first,
                                      const endstone::Location &second, Flag flag)
{
    return allowed(player, first, flag) && allowed(player, second, flag);
}

bool ProtectionContext::allowedAtBoth(const endstone::Location &first, const endstone::Location &second, Flag flag)
{
    return allowed(first, flag) && allowed(second, flag);
}

bool ProtectionContext::allowedTransition(endstone::Player &player, const endstone::Location &from,
                                          const endstone::Location &to)
{
    const auto from_dimension = targetDimension(from);
    const auto to_dimension = targetDimension(to);
    const auto from_position = blockPosition(from);
    const auto to_position = blockPosition(to);
    if (from_dimension == to_dimension && from_position == to_position) {
        return true;
    }
    const auto *service = plugin_.getService();
    if (!service) {
        plugin_.getMessenger().deny(player, Message::NotReady);
        return false;
    }
    if (service->getRegions().isTransitionAllowed(from_dimension, from_position, to_dimension, to_position,
                                                  player.getUniqueId().str(),
                                                  player.hasPermission("dimenguard.bypass"))) {
        return true;
    }
    plugin_.getMessenger().deny(player);
    return false;
}

bool ProtectionContext::chestNeighbors(endstone::Player &player, const endstone::Block &block)
{
    const auto type = block.getType().getId();
    if (type != endstone::BlockTypeId::minecraft("chest") &&
        type != endstone::BlockTypeId::minecraft("trapped_chest")) {
        return true;
    }
    const auto dimension = block.getDimension();
    constexpr std::array<BlockPosition, 4> offsets{{{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}}};
    for (const auto &offset : offsets) {
        const auto x = checkedOffset(block.getX(), offset.x);
        const auto z = checkedOffset(block.getZ(), offset.z);
        if (!dimension->isChunkLoaded(chunkCoordinate(x), chunkCoordinate(z))) {
            throw std::runtime_error("Cannot verify an adjacent chest in an unloaded chunk");
        }
        const auto neighbor = dimension->getBlockAt(x, block.getY(), z);
        // Public Endstone does not expose chest pairing. Adjacent same-type chests can be
        // denied even when unpaired, keeping protected double chests inaccessible.
        if (neighbor->getType().getId() == type && !allowed(player, neighbor->getLocation(), Flag::ContainerAccess)) {
            return false;
        }
    }
    return true;
}

void ProtectionContext::reportFailure(std::string_view message) noexcept
{
    const auto now = std::chrono::steady_clock::now();
    if (error_reported_ && now - last_error_ < error_interval) {
        return;
    }
    error_reported_ = true;
    last_error_ = now;
    try {
        plugin_.getLogger().error("Protection listener failed: {}", message);
    }
    catch (...) {
        // Logging must never escape an event handler after its action has been cancelled.
    }
}

}
