#include "dimenguard/presentation/panel.h"

#include <format>
#include <utility>

namespace dimenguard {
namespace {

std::string panelRule()
{
    return std::format("{}------------------------------------------{}", Theme::DarkGray, Theme::Reset);
}

}

PanelBuilder::PanelBuilder(std::string_view title, PanelStyle style) : style_(style)
{
    if (style_ == PanelStyle::Framed) {
        lines_.push_back(panelRule());
    }
    lines_.push_back(Theme::decorate(title));
    if (style_ == PanelStyle::Framed) {
        lines_.push_back(panelRule());
    }
}

void PanelBuilder::heading(std::string_view text)
{
    lines_.emplace_back(Theme::Reset);
    line(text, Theme::Amethyst, "  ");
}

void PanelBuilder::entry(std::string_view label, std::string_view description, std::string_view label_color)
{
    lines_.push_back(std::format("  {}{}{} / {}{}{}", label_color, label, Theme::DarkGray, Theme::LightGray,
                                 description, Theme::Reset));
}

void PanelBuilder::line(std::string_view text, std::string_view color, std::string_view indent)
{
    lines_.push_back(std::format("{}{}{}{}", indent, color, text, Theme::Reset));
}

std::vector<std::string> PanelBuilder::finish() &&
{
    if (style_ == PanelStyle::Framed) {
        lines_.push_back(panelRule());
    }
    return std::move(lines_);
}

}
