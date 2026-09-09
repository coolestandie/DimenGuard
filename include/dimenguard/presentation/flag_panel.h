#pragma once

#include "dimenguard/i18n/translator.h"
#include "dimenguard/region/region.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace dimenguard {

struct FlagPanelPage {
    std::vector<std::string> lines;
    std::size_t page;
    std::size_t page_count;
    std::size_t total_count;
};

[[nodiscard]] std::optional<FlagPanelPage> renderFlagCatalogPage(Locale locale, std::size_t page = 1,
                                                                 bool can_manage = false,
                                                                 std::optional<FlagScope> scope = std::nullopt);
[[nodiscard]] std::vector<std::string> renderFlagCatalog(Locale locale);
[[nodiscard]] std::string displayFlagValue(const FlagValue &value);
[[nodiscard]] std::vector<std::string> renderRegionFlags(const Region &region, Locale locale,
                                                         std::optional<Flag> selected = std::nullopt);

}
