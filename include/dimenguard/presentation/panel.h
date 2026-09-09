#pragma once

#include "dimenguard/presentation/theme.h"

#include <string>
#include <string_view>
#include <vector>

namespace dimenguard {

enum class PanelStyle {
    Framed,
    Compact
};

class PanelBuilder {
public:
    explicit PanelBuilder(std::string_view title, PanelStyle style = PanelStyle::Framed);

    void heading(std::string_view text);
    void entry(std::string_view label, std::string_view description, std::string_view label_color = Theme::Amethyst);
    void line(std::string_view text, std::string_view color = Theme::LightGray, std::string_view indent = {});
    [[nodiscard]] std::vector<std::string> finish() &&;

private:
    std::vector<std::string> lines_;
    PanelStyle style_;
};

}
