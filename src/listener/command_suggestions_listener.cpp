#include "dimenguard/listener/command_suggestions_listener.h"

#include "dimenguard/adapter/context.h"
#include "dimenguard/command/permissions.h"
#include "dimenguard/plugin.h"
#include "dimenguard/protocol/command_suggestions.h"

#include <endstone/command/plugin_command.h>
#include <endstone/event/player/player_dimension_change_event.h>
#include <endstone/event/server/packet_send_event.h>
#include <endstone/player.h>
#include <endstone/scheduler/scheduler.h>
#include <endstone/server.h>
#include <vector>

namespace dimenguard {
CommandSuggestionsListener::CommandSuggestionsListener(DimenGuardPlugin &plugin) : plugin_(plugin)
{
    supported_ = plugin_.getServer().getProtocolVersion() == 2169;
}

CommandSuggestionsListener::~CommandSuggestionsListener()
{
    // onDisable runs before Endstone cancels the plugin's scheduled callbacks.
    if (refresh_task_) {
        plugin_.getServer().getScheduler().cancelTask(*refresh_task_);
    }
}

void CommandSuggestionsListener::registerEvents()
{
    if (!supported_) {
        plugin_.getLogger().warning("Live region suggestions require protocol 2169; commands remain available.");
        return;
    }
    plugin_.registerEvent<endstone::PacketSendEvent>([this](endstone::PacketSendEvent &event) { onPacketSend(event); },
                                                     endstone::EventPriority::Highest, true);
    plugin_.registerEvent<endstone::PlayerDimensionChangeEvent>(
        [this](endstone::PlayerDimensionChangeEvent &) { requestRefresh(); });
}

void CommandSuggestionsListener::requestRefresh() noexcept
{
    if (!supported_ || refresh_task_) {
        return;
    }
    try {
        const auto task = plugin_.getServer().getScheduler().runTaskLater(
            plugin_,
            [this] {
                refresh_task_.reset();
                refresh();
            },
            1);
        if (task) {
            refresh_task_ = task->getTaskId();
        }
        else {
            reportFailure();
        }
    }
    catch (...) {
        reportFailure();
    }
}

void CommandSuggestionsListener::refresh() noexcept
{
    try {
        for (const auto &player : plugin_.getServer().getOnlinePlayers()) {
            if (hasAnyCommandPermission(*player, CommandPermission::List) ||
                hasAnyCommandPermission(*player, CommandPermission::Info)) {
                player->updateCommands();
            }
        }
    }
    catch (...) {
        reportFailure();
    }
}

void CommandSuggestionsListener::onPacketSend(endstone::PacketSendEvent &event) noexcept
{
    if (event.getPacketId() != 76 || !event.getPlayer()) {
        return;
    }
    try {
        const auto &player = event.getPlayer();
        const auto command = plugin_.getCommand("dg");
        if (!command || &command->getPlugin() != &plugin_) {
            return;
        }
        const auto *service = plugin_.getService();
        if (!service) {
            return;
        }
        const auto regions = service->getRegions().inDimension(dimensionKey(*player->getDimension()));
        std::vector<std::string> names;
        names.reserve(regions.size());
        for (const auto *region : regions) {
            if (canSeeRegion(*player, *region)) {
                names.push_back(region->key.name);
            }
        }
        if (names.empty() && !hasAnyCommandPermission(*player, CommandPermission::List) &&
            !hasAnyCommandPermission(*player, CommandPermission::Info)) {
            return;
        }
        if (const auto payload = rewriteCommandSuggestions(event.getPayload(), names)) {
            event.setPayload(*payload);
        }
        else {
            reportFailure();
        }
    }
    catch (...) {
        reportFailure();
    }
}

void CommandSuggestionsListener::reportFailure() noexcept
{
    if (failure_reported_) {
        return;
    }
    failure_reported_ = true;
    try {
        plugin_.getLogger().warning("Could not update live region suggestions; retaining the original command packet.");
    }
    catch (...) {
    }
}
}
