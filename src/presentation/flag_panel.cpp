#include "dimenguard/presentation/flag_panel.h"

#include "dimenguard/command/catalog.h"
#include "dimenguard/presentation/panel.h"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

namespace dimenguard {
namespace {

Message flagDescription(Flag flag)
{
    switch (flag) {
    case Flag::Build:
        return Message::FlagBuildDescription;
    case Flag::Interact:
        return Message::FlagInteractDescription;
    case Flag::ContainerAccess:
        return Message::FlagContainerDescription;
    case Flag::Pvp:
        return Message::FlagPvpDescription;
    case Flag::Explosions:
        return Message::FlagExplosionsDescription;
    case Flag::FluidFlow:
        return Message::FlagFluidFlowDescription;
    case Flag::BlockForm:
        return Message::FlagBlockFormDescription;
    case Flag::LeafDecay:
        return Message::FlagLeafDecayDescription;
    case Flag::ActorGriefing:
        return Message::FlagActorGriefingDescription;
    case Flag::MobSpawning:
        return Message::FlagMobSpawningDescription;
    case Flag::MobDamage:
        return Message::FlagMobDamageDescription;
    case Flag::Entry:
        return Message::FlagEntryDescription;
    case Flag::Exit:
        return Message::FlagExitDescription;
    }
    throw std::invalid_argument("A supported flag has no description");
}

}

std::vector<std::string> renderFlagCatalog(Locale locale)
{
    PanelBuilder panel(messageText(Message::FlagsAvailable, locale));
    for (const auto flag : supportedFlags()) {
        panel.entry(flagName(flag), messageText(flagDescription(flag), locale));
    }
    std::string states;
    for (const auto state : supportedFlagStates()) {
        if (!states.empty()) {
            states += ", ";
        }
        states += stateName(state);
    }
    panel.line(translate(Message::FlagStates, locale, states), Theme::White);
    const auto commands = commandCatalog();
    const auto command = std::ranges::find(commands, std::string_view{"flag"}, &CommandSpec::path);
    if (command == commands.end()) {
        throw std::logic_error("The flag command is missing from the command catalog");
    }
    panel.line(translate(Message::FlagSyntax, locale, helpUsage(*command)));
    panel.line(messageText(Message::FlagExample, locale));
    panel.line(messageText(Message::FlagDefaults, locale));
    return std::move(panel).finish();
}

std::vector<std::string> renderRegionFlags(const Region &region, Locale locale, std::optional<Flag> selected)
{
    if (selected) {
        static_cast<void>(flagName(*selected));
    }
    PanelBuilder panel(translate(Message::RegionFlags, locale, region.key.name), PanelStyle::Compact);
    for (const auto flag : supportedFlags()) {
        if (selected && flag != *selected) {
            continue;
        }
        const auto found = region.flags.find(flag);
        const auto state = found == region.flags.end() ? FlagState::Inherit : found->second;
        const auto value = std::format("{}{}", Theme::LightGray, stateName(state));
        panel.line(translate(Message::FlagInfo, locale, flagName(flag), value), Theme::White, "  ");
    }
    panel.line(messageText(Message::FlagInheritance, locale));
    panel.line(messageText(Message::FlagDefaults, locale));
    return std::move(panel).finish();
}

}
