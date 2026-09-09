#pragma once
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace dimenguard {
[[nodiscard]] std::optional<std::vector<std::string>> normalizeCommandArguments(std::span<const std::string> args);
}
