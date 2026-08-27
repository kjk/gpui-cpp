#ifndef GPUI_BASE_TABS_H_
#define GPUI_BASE_TABS_H_
/* Unstyled tabs — crates/base/src/tabs.rs */

#include "base/state_style.h"

namespace gpui {

struct TabStyles {
    StateStyle selected = {};
    StateStyle disabled = {};

    TabStyles& Selected(const StateStyle& style);
    TabStyles& Disabled(const StateStyle& style);
};

// A collection root for tabs. Selection and activation live on the children,
// so the root is identity and nothing else.
struct Tabs {
    static El* New(Ctx* cx, Str id);
};

// Rust's `Tab::new(id).disabled(..).on_click(..)`. A tab does not take
// keyboard focus of its own — Rust says so outright, leaving that to a
// compound tab list that does not exist yet — so it gets identity and the
// click and no FocusId. `onClick` is passed through untouched: a tab produces
// no value of its own, so whichever index the caller bound is what its handler
// reads. Selection, naming and set position are the semantic builder fields
// Rust projects while rendering.
struct Tab {
    static El* New(Ctx* cx, Str id, bool disabled = false,
                   Listener onClick = {}, bool selected = false,
                   Str accessibilityLabel = {}, int positionInSet = 0,
                   int sizeOfSet = 0, const TabStyles* styles = nullptr,
                   const StateStyle* instance = nullptr);
};
} // namespace gpui
#endif // GPUI_BASE_TABS_H_
