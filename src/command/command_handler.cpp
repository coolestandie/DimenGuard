#include "dimenguard/command/command_handler.h"

#include "dimenguard/command/arguments.h"
#include "dimenguard/command/handlers.h"
#include "dimenguard/plugin.h"
namespace dimenguard {
namespace {
Message serviceMessage(ServiceErrorCode code)
{
    switch (code) {
    case ServiceErrorCode::NotFound:
        return Message::NotFound;
    case ServiceErrorCode::Exists:
        return Message::Exists;
    case ServiceErrorCode::InvalidName:
        return Message::InvalidName;
    case ServiceErrorCode::InvalidBounds:
        return Message::InvalidBounds;
    case ServiceErrorCode::LimitReached:
        return Message::LimitReached;
    case ServiceErrorCode::HasChildren:
        return Message::RegionHasChildren;
    case ServiceErrorCode::InvalidHierarchy:
        return Message::InvalidHierarchy;
    case ServiceErrorCode::InvalidRegionType:
        return Message::InvalidRegionType;
    }
    return Message::Failed;
}
}
CommandHandler::CommandHandler(DimenGuardPlugin &plugin) : plugin_(plugin), context_(plugin) {}
void CommandHandler::execute(endstone::CommandSender &sender, std::span<const std::string> args)
{
    try {
        auto normalized = normalizeCommandArguments(args);
        requireArgument(normalized.has_value());
        const auto revision = plugin_.getService() ? plugin_.getService()->getRegionNamesRevision() : 0;
        dispatch(sender, *normalized);
        if (plugin_.getService() && revision != plugin_.getService()->getRegionNamesRevision()) {
            plugin_.refreshRegionSuggestions();
        }
    }
    catch (const CommandError &error) {
        context_.messages().send(sender, error.getMessage());
    }
    catch (const ServiceError &error) {
        context_.messages().send(sender, serviceMessage(error.getCode()));
    }
    catch (const std::exception &error) {
        plugin_.getLogger().error("Command failed: {}", error.what());
        context_.messages().send(sender, Message::Failed);
    }
}
void CommandHandler::dispatch(endstone::CommandSender &sender, std::span<const std::string> args)
{
    if (args.empty() || args[0] == "help" || args[0] == "language" || args[0] == "flags") {
        executeGeneralCommand(context_, sender, args);
        return;
    }
    requireArgument(sender.hasPermission("dimenguard.command"), Message::NoPermission);
    if (args[0] == "reload") {
        executeGeneralCommand(context_, sender, args);
    }
    else if (args[0] == "pos1" || args[0] == "pos2" || args[0] == "inspect") {
        executeSelectionCommand(context_, sender, args);
    }
    else if (args[0] == "region") {
        executeRegionCommand(context_, sender, args.subspan(1));
    }
    else if (args[0] == "flag") {
        executeFlagCommand(context_, sender, args.subspan(1));
    }
    else if (args[0] == "trust" || args[0] == "untrust") {
        executeMembershipCommand(context_, sender, args);
    }
    else {
        throw CommandError(Message::Usage);
    }
}
}
