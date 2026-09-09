#include "dimenguard/presentation/help_panel.h"

#include "dimenguard/command/catalog.h"
#include "dimenguard/presentation/panel.h"

#include <format>
#include <utility>

namespace dimenguard {
namespace {

void appendSection(PanelBuilder &panel, Locale locale, bool can_manage, CommandSection section, Message heading)
{
    bool started = false;
    for (const auto &entry : commandCatalog()) {
        if (entry.section != section || (entry.requires_admin && !can_manage)) {
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

std::vector<std::string> renderHelp(Locale locale, bool can_manage)
{
    PanelBuilder panel(messageText(Message::Help, locale));
    appendSection(panel, locale, can_manage, CommandSection::Selection, Message::HelpSelection);
    appendSection(panel, locale, can_manage, CommandSection::Regions, Message::HelpRegions);
    appendSection(panel, locale, can_manage, CommandSection::Protection, Message::HelpProtection);
    appendSection(panel, locale, can_manage, CommandSection::General, Message::HelpGeneral);
    return std::move(panel).finish();
}

}
