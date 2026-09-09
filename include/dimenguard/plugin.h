#pragma once

#include "dimenguard/command/selection.h"
#include "dimenguard/presentation/messenger.h"
#include "dimenguard/service/region_service.h"

#include <endstone/plugin/plugin.h>
#include <memory>

namespace dimenguard {

class CommandHandler;
class ProtectionListener;
class CommandSuggestionsListener;

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
    void refreshRegionSuggestions() noexcept;

private:
    std::unique_ptr<RegionService> service_;
    Messenger messenger_;
    SelectionManager selections_;
    std::unique_ptr<CommandHandler> commands_;
    std::unique_ptr<ProtectionListener> protection_;
    std::unique_ptr<CommandSuggestionsListener> suggestions_;
};

}
