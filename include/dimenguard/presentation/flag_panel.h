#pragma once

#include "dimenguard/i18n/translator.h"
#include "dimenguard/region/region.h"

#include <optional>
#include <string>
#include <vector>

namespace dimenguard {

[[nodiscard]] std::vector<std::string> renderFlagCatalog(Locale locale);
[[nodiscard]] std::vector<std::string> renderRegionFlags(const Region &region, Locale locale,
                                                         std::optional<Flag> selected = std::nullopt);

}
