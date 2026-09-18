#pragma once

#include "dimenguard/region/flag.h"

#include <optional>
#include <string_view>

namespace dimenguard {

struct FlagChange {
    std::optional<FlagValue> value;
};

[[nodiscard]] std::optional<FlagChange> parseFlagChange(Flag flag, std::string_view text);

}
