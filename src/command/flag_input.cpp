#include "dimenguard/command/flag_input.h"

#include <utility>

namespace dimenguard {

std::optional<FlagChange> parseFlagChange(Flag flag, std::string_view text)
{
    const auto type = flagType(flag);
    if (text == "--unset" || (type == FlagType::State && text == "inherit")) {
        return FlagChange{};
    }
    auto value = parseFlagValue(flag, text);
    return value ? std::optional{FlagChange{std::move(value)}} : std::nullopt;
}

}
