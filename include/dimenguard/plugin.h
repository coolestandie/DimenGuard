#pragma once

#include "dimenguard/command/selection.h"
#include "dimenguard/i18n/messenger.h"
#include "dimenguard/service/region_service.h"

#include <endstone/plugin/plugin.h>
#include <memory>

namespace dimenguard {

class CommandHandler;

class DimenGuardPlugin : public endstone::Plugin {
public:
    DimenGuardPlugin();
    ~DimenGuardPlugin() override;
    void onEnable() override;
    void onDisable() override;
    bool onCommand(const endstone::NotNull<endstone::CommandSender> &sender, const endstone::Command &command,
                   const std::vector<std::string> &args) override;

    [[nodiscard]] RegionService *getService() const { return service_.get(); }
    [[nodiscard]] Messenger &getMessenger() { return messenger_; }
    [[nodiscard]] SelectionManager &getSelections() { return selections_; }
    void reloadRegions();

private:
    std::unique_ptr<RegionService> service_;
    Messenger messenger_;
    SelectionManager selections_;
    std::unique_ptr<CommandHandler> commands_;
};

}  // namespace dimenguard
