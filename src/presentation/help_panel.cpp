#include "dimenguard/presentation/help_panel.h"

#include "dimenguard/command/catalog.h"
#include "dimenguard/command/permissions.h"
#include "dimenguard/presentation/panel.h"

#include <format>
#include <utility>

namespace dimenguard {
namespace {

void appendSection(PanelBuilder &panel, Locale locale, const HelpPermissionCheck &can_use, CommandSection section,
                   Message heading)
{
    bool started = false;
    for (const auto &entry : commandCatalog()) {
        const auto permission = permissionForCommand(entry.path);
        if (entry.section != section || (permission && !can_use(permissionName(*permission)))) {
            continue;
        }
        if (!started) {
            panel.heading(messageText(heading, locale));
            started = true;
        }
        const auto usage = helpUsage(entry);
        const auto argument_start = usage.find_first_of("<[");
        const auto command = std::string_view(usage).substr(0, argument_start);
        const auto arguments =
            argument_start == usage.npos ? std::string_view{} : std::string_view(usage).substr(argument_start);
        panel.entry(std::format("{}{}{}", command, Theme::LightGray, arguments), messageText(entry.description, locale),
                    Theme::White);
    }
}

}

std::vector<std::string> renderHelp(Locale locale, const HelpPermissionCheck &can_use)
{
    PanelBuilder panel(messageText(Message::Help, locale));
    appendSection(panel, locale, can_use, CommandSection::Selection, Message::HelpSelection);
    appendSection(panel, locale, can_use, CommandSection::Regions, Message::HelpRegions);
    appendSection(panel, locale, can_use, CommandSection::Protection, Message::HelpProtection);
    appendSection(panel, locale, can_use, CommandSection::General, Message::HelpGeneral);
    return std::move(panel).finish();
}

std::vector<std::string> renderHelp(Locale locale, bool can_manage)
{
    return renderHelp(locale, [can_manage](std::string_view) { return can_manage; });
}

}
