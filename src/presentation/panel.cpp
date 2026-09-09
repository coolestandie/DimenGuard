#include "dimenguard/presentation/panel.h"

#include <algorithm>
#include <format>
#include <utility>

namespace dimenguard {
namespace {

std::string panelRule()
{
    return std::format("{}------------------------------------------{}", Theme::DarkGray, Theme::Reset);
}

}

std::optional<PageSlice> paginate(std::size_t total_count, std::size_t page, std::size_t page_size)
{
    if (page == 0 || page_size == 0) {
        return std::nullopt;
    }
    const auto page_count = total_count == 0 ? 1 : (total_count - 1) / page_size + 1;
    if (page > page_count) {
        return std::nullopt;
    }
    const auto offset = (page - 1) * page_size;
    return PageSlice{offset, std::min(page_size, total_count - offset), page, page_count};
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
