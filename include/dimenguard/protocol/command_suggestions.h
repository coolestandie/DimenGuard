#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace dimenguard {

/** Rewrites only dg's client grammar in a headerless AvailableCommands payload for protocol 2169.
 * Returns nullopt for malformed, oversized, unsupported or non-dg payloads; callers retain the original packet.
 * This does not register commands or change server-side command validation.
 */
[[nodiscard]] std::optional<std::string> rewriteCommandSuggestions(std::string_view payload,
                                                                   std::span<const std::string> regions);

}
