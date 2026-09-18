#include "dimenguard/command/arguments.h"

#include "dimenguard/command/catalog.h"
#include "dimenguard/command/parse.h"

namespace dimenguard {
namespace {

std::size_t regionArgumentLimit(std::string_view action)
{
    std::string path{"region "};
    path += action;
    for (const auto &command : commandCatalog()) {
        if (command.path == path) {
            return command.parameters.size() + 2;
        }
        for (const auto alias : commandAliases(command.path)) {
            if (alias == action) {
                return command.parameters.size() + 2;
            }
        }
    }
    return 6;
}

}

std::optional<std::vector<std::string>> normalizeCommandArguments(std::span<const std::string> args)
{
    if (args.empty()) {
        return std::vector<std::string>{};
    }
    const auto &root = args.front();
    const bool region = root == "region";
    const bool flag = root == "flag";
    const bool membership = root == "trust" || root == "untrust";
    if (!region && !flag && !membership) {
        return std::vector<std::string>{args.begin(), args.end()};
    }
    const std::size_t prefix_size = region ? 2 : 1;
    if (args.size() < prefix_size || args.size() > prefix_size + 1) {
        return std::nullopt;
    }
    const auto tail = args.size() > prefix_size ? std::string_view(args.back()) : std::string_view{};
    const auto max_count = region ? regionArgumentLimit(args[1]) : flag ? 3 : 2;
    const auto message_tail = flag;
    auto words = parseCommandArguments(tail, max_count, message_tail);
    if (!words) {
        return std::nullopt;
    }
    words->insert(words->begin(), args.begin(), args.begin() + static_cast<std::ptrdiff_t>(prefix_size));
    return words;
}
}
