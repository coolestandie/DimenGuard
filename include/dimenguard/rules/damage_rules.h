#pragma once

#include "dimenguard/region/flag.h"

#include <optional>
#include <string_view>

namespace dimenguard {

[[nodiscard]] std::optional<Flag> damageFlag(std::string_view damage_type);

}
