#include "dimenguard/plugin.h"

#include "dimenguard/command/command_handler.h"
#include "dimenguard/listener/protection_listener.h"
#include "dimenguard/version.h"

#include <endstone/event/player/player_quit_event.h>
#include <exception>

namespace dimenguard {

DimenGuardPlugin::DimenGuardPlugin() = default;
DimenGuardPlugin::~DimenGuardPlugin() = default;

void DimenGuardPlugin::onEnable()
{
    commands_ = std::make_unique<CommandHandler>(*this);
    protection_ = std::make_unique<ProtectionListener>(*this);
    protection_->registerEvents();
    registerEvent<endstone::PlayerQuitEvent>([this](endstone::PlayerQuitEvent &event) {
        messenger_.forget(*event.getPlayer());
        selections_.forget(event.getPlayer()->getUniqueId().str());
    });
    try {
        reloadRegions();
        getLogger().info("Loaded {} regions.", service_->getRegions().getAll().size());
    }
    catch (const std::exception &error) {
        getLogger().error("Region storage is unavailable: {}. Intercepted actions are locked globally. "
                          "Repair storage and run /dg reload.",
                          error.what());
    }
}

void DimenGuardPlugin::onDisable()
{
    commands_.reset();
    protection_.reset();
    selections_ = {};
    messenger_ = {};
    service_.reset();
}

bool DimenGuardPlugin::onCommand(const endstone::NotNull<endstone::CommandSender> &sender, const endstone::Command &,
                                 const std::vector<std::string> &args)
{
    if (!commands_) {
        messenger_.send(*sender, Message::NotReady);
        return true;
    }
    commands_->execute(*sender, args);
    return true;
}

void DimenGuardPlugin::reloadRegions()
{
    if (service_) {
        service_->reload();
    }
    else {
        service_ = std::make_unique<RegionService>(getDataFolder() / "regions.sqlite3");
    }
}

}  // namespace dimenguard

ENDSTONE_PLUGIN("dimenguard", DIMENGUARD_VERSION, dimenguard::DimenGuardPlugin)
{
    description = "Dimension-scoped region protection.";
    authors = {"coolestandie"};
    prefix = "DimenGuard";
    command("dg")
        .description("Manage DimenGuard regions and language.")
        .aliases("dimenguard")
        .usages("/dg [action: str] [argument1: str] [argument2: str] [argument3: str]")
        .permissions("dimenguard.use");
    permission("dimenguard.use")
        .description("Use help and language commands.")
        .default_(endstone::PermissionDefault::True);
    permission("dimenguard.command")
        .description("Manage regions, flags and membership.")
        .default_(endstone::PermissionDefault::Operator);
    permission("dimenguard.bypass")
        .description("Bypass region protection. Explicit grant required, including operators.")
        .default_(endstone::PermissionDefault::False);
}
