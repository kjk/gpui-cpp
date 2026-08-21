#include "base/list_settings.h"

namespace gpui {

static ListSettings gListSettings;

const ListSettings& ListSettingsNow() {
    return gListSettings;
}

void ListSettingsSet(ListSettings s) {
    gListSettings = s;
}

ListActiveStyle ListActiveStyleOf(Background active, Rgba activeBorder,
                                  Background accent, bool selected) {
    ListActiveStyle out;
    bool highlight = gListSettings.activeHighlight;
    // list_item.rs: the tint is for the selection proper — a row marked by a
    // right press takes `accent` either way — and the rule comes with the
    // setting, not with the row's state.
    out.bg = (selected && highlight) ? active : accent;
    out.border = activeBorder;
    out.hasBorder = highlight;
    return out;
}

El* ListActiveOverlay(Arena* a, Rgba border, float radius) {
    return Div(a)
        ->Absolute()
        ->Top(0)
        ->Left(0)
        ->Right(0)
        ->Bottom(0)
        ->Radius(radius)
        ->Border(1, border);
}

} // namespace gpui
