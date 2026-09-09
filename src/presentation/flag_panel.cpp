#include "dimenguard/presentation/flag_panel.h"

#include "dimenguard/command/catalog.h"
#include "dimenguard/presentation/panel.h"

#include <algorithm>
#include <array>
#include <format>
#include <span>
#include <stdexcept>
#include <utility>

namespace dimenguard {
namespace {

constexpr std::size_t flag_page_size = 6;
constexpr std::array scopes{FlagScope::Player, FlagScope::Environment, FlagScope::Transition};

Message scopeHeading(FlagScope scope)
{
    switch (scope) {
    case FlagScope::Player:
        return Message::FlagScopePlayer;
    case FlagScope::Environment:
        return Message::FlagScopeEnvironment;
    case FlagScope::Transition:
        return Message::FlagScopeTransition;
    }
    throw std::invalid_argument("Unknown flag scope");
}

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
    case Flag::BlockBreak:
        return Message::FlagBlockBreakDescription;
    case Flag::BlockPlace:
        return Message::FlagBlockPlaceDescription;
    case Flag::Use:
        return Message::FlagUseDescription;
    case Flag::UseAnvil:
        return Message::FlagUseAnvilDescription;
    case Flag::Sleep:
        return Message::FlagSleepDescription;
    case Flag::ItemDrop:
        return Message::FlagItemDropDescription;
    case Flag::ItemPickup:
        return Message::FlagItemPickupDescription;
    case Flag::SendChat:
        return Message::FlagSendChatDescription;
    case Flag::WaterFlow:
        return Message::FlagWaterFlowDescription;
    case Flag::LavaFlow:
        return Message::FlagLavaFlowDescription;
    case Flag::FallDamage:
        return Message::FlagFallDamageDescription;
    case Flag::FireworkDamage:
        return Message::FlagFireworkDamageDescription;
    case Flag::Invincible:
        return Message::FlagInvincibleDescription;
    }
    throw std::invalid_argument("A supported flag has no description");
}

const CommandSpec &commandByPath(std::string_view path)
{
    const auto commands = commandCatalog();
    const auto command = std::ranges::find(commands, path, &CommandSpec::path);
    if (command == commands.end()) {
        throw std::logic_error("A flag command is missing from the command catalog");
    }
    return *command;
}

void appendPolicyNotes(PanelBuilder &panel, Locale locale)
{
    panel.line(messageText(Message::FlagDefaults, locale));
    panel.line(messageText(Message::FlagFallbacks, locale));
    panel.line(messageText(Message::FlagImmunityDefault, locale));
}

void appendCatalogFooter(PanelBuilder &panel, Locale locale, bool can_manage)
{
    std::string states;
    for (const auto state : supportedFlagStates()) {
        if (!states.empty()) {
            states += ", ";
        }
        states += stateName(state);
    }
    panel.line(translate(Message::FlagStates, locale, states), Theme::White);
    if (can_manage) {
        panel.line(translate(Message::FlagSyntax, locale, helpUsage(commandByPath("flag"))));
        panel.line(messageText(Message::FlagExample, locale));
    }
    appendPolicyNotes(panel, locale);
}

void appendNavigation(PanelBuilder &panel, Locale locale, const PageSlice &page)
{
    if (page.page_count <= 1) {
        return;
    }
    const auto &command = commandByPath("flags");
    std::string navigation;
    if (page.page > 1) {
        navigation = std::format("/dg {} {}", command.path, page.page - 1);
    }
    if (page.page < page.page_count) {
        if (!navigation.empty()) {
            navigation += " | ";
        }
        navigation += std::format("/dg {} {}", command.path, page.page + 1);
    }
    panel.line(translate(Message::FlagNavigation, locale, navigation));
}

}

std::optional<FlagPanelPage> renderFlagCatalogPage(Locale locale, std::size_t page, bool can_manage,
                                                   std::optional<FlagScope> scope)
{
    if (scope && std::ranges::find(scopes, *scope) == scopes.end()) {
        return std::nullopt;
    }
    std::vector<Flag> flags;
    for (const auto candidate : scopes) {
        if (scope && *scope != candidate) {
            continue;
        }
        for (const auto flag : supportedFlags()) {
            if (flagScope(flag) == candidate) {
                flags.push_back(flag);
            }
        }
    }
    const auto slice = paginate(flags.size(), page, flag_page_size);
    if (!slice) {
        return std::nullopt;
    }
    PanelBuilder panel(translate(Message::FlagsPage, locale, messageText(Message::FlagsAvailable, locale), slice->page,
                                 slice->page_count, flags.size()));
    std::optional<FlagScope> current_scope;
    for (const auto flag : std::span(flags).subspan(slice->offset, slice->count)) {
        const auto category = flagScope(flag);
        if (category != current_scope) {
            panel.heading(messageText(scopeHeading(category), locale));
            current_scope = category;
        }
        panel.entry(flagName(flag), messageText(flagDescription(flag), locale));
    }
    appendCatalogFooter(panel, locale, can_manage);
    if (!scope) {
        appendNavigation(panel, locale, *slice);
    }
    return FlagPanelPage{std::move(panel).finish(), slice->page, slice->page_count, flags.size()};
}

std::vector<std::string> renderFlagCatalog(Locale locale)
{
    auto page = renderFlagCatalogPage(locale, 1, true);
    return std::move(page->lines);
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
    appendPolicyNotes(panel, locale);
    return std::move(panel).finish();
}

}
