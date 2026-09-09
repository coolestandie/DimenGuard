#pragma once

#include "dimenguard/region/flag.h"

#include <optional>
#include <string_view>

namespace dimenguard {

[[nodiscard]] std::optional<Flag> blockUseFlag(std::string_view block_namespace, std::string_view block_key);
[[nodiscard]] Flag fluidFlowFlag(std::string_view block_namespace, std::string_view block_key);

}
