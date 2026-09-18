#include "dimenguard/plugin.h"

#include "dimenguard/command/catalog.h"
#include "dimenguard/command/command_handler.h"
#include "dimenguard/command/permissions.h"
#include "dimenguard/listener/command_suggestions_listener.h"
#include "dimenguard/listener/protection_listener.h"
#include "dimenguard/region/flag.h"
#include "dimenguard/version.h"

#include <exception>
#include <string>
#include <string_view>

namespace dimenguard {

DimenGuardPlugin::DimenGuardPlugin() = default;
DimenGuardPlugin::~DimenGuardPlugin() = default;

void DimenGuardPlugin::onEnable()
{
    commands_ = std::make_unique<CommandHandler>(*this);
    protection_ = std::make_unique<ProtectionListener>(*this);
    protection_->registerEvents();
    suggestions_ = std::make_unique<CommandSuggestionsListener>(*this);
    suggestions_->registerEvents();
    try {
        reloadRegions();
        getLogger().info("Loaded {} regions.", service_->getRegions().getAll().size());
        refreshRegionSuggestions();
    }
    catch (const std::exception &error) {
        getLogger().error("Region storage is unavailable: {}. Intercepted actions are locked globally. "
                          "Repair storage and run /dg reload.",
                          error.what());
    }
}

void DimenGuardPlugin::onDisable()
{
    suggestions_.reset();
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

void DimenGuardPlugin::refreshRegionSuggestions() noexcept
{
    if (suggestions_) {
        suggestions_->requestRefresh();
    }
}

}

ENDSTONE_PLUGIN("dimenguard", DIMENGUARD_VERSION, dimenguard::DimenGuardPlugin)
{
    description = "Dimension-scoped region protection.";
    authors = {"coolestandie"};
    prefix = "DimenGuard";
    auto &root_command = command("dg")
                             .description("Manage DimenGuard regions and language.")
                             .aliases("dimenguard")
                             .permissions("dimenguard.use");
    for (const auto &usage : dimenguard::nativeUsages()) {
        root_command.usages(usage);
    }
    permission("dimenguard.use")
        .description("Use help, flag discovery and language commands.")
        .default_(endstone::PermissionDefault::True);

    auto &legacy_admin = permission("dimenguard.command")
                             .description("Legacy aggregate for all DimenGuard administration nodes.")
                             .default_(endstone::PermissionDefault::Operator);
    for (const auto command_permission : dimenguard::commandPermissions()) {
        const auto name = std::string(dimenguard::permissionName(command_permission));
        legacy_admin.children(name, true);
        permission(name)
            .description("Use the DimenGuard command permission for " + name + ".")
            .default_(endstone::PermissionDefault::False);
        permission(name + ".own")
            .description("Use the command on regions owned by the sender.")
            .default_(endstone::PermissionDefault::False);
        permission(name + ".member")
            .description("Use the command on regions where the sender is a member.")
            .default_(endstone::PermissionDefault::False);
        permission(name + ".region.*")
            .description("Use the command on explicitly assigned regions.")
            .default_(endstone::PermissionDefault::False);
    }
    permission("dimenguard.region.flag.*")
        .description("Set any registered flag when the region scope also permits it.")
        .default_(endstone::PermissionDefault::False);
    for (const auto flag : dimenguard::supportedFlags()) {
        const auto flag_permission = dimenguard::flagPermissionName(flag);
        permission(flag_permission)
            .description("Set the " + std::string(dimenguard::flagName(flag)) + " flag.")
            .default_(endstone::PermissionDefault::False);
        for (const auto value : {std::string_view{"allow"}, std::string_view{"deny"}, std::string_view{"inherit"},
                                 std::string_view{"set"}, std::string_view{"unset"}}) {
            permission(dimenguard::flagValuePermissionName(flag, value))
                .description("Set " + std::string(dimenguard::flagName(flag)) + " to " + std::string(value) + ".")
                .default_(endstone::PermissionDefault::False);
        }
    }
    permission("dimenguard.recovery")
        .description("Reload storage and recover administration after a failure.")
        .default_(endstone::PermissionDefault::Operator);
    permission("dimenguard.bypass")
        .description("Bypass region protection. Explicit grant required, including operators.")
        .default_(endstone::PermissionDefault::False);
}
