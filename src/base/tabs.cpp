#include "base/tabs.h"

namespace gpui {

El* Tabs::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id)->Role(AccessibilityRole::TabList);
}

El* Tab::New(Ctx* cx, Str id, bool disabled, Listener onClick, bool selected,
             Str accessibilityLabel, int positionInSet, int sizeOfSet) {
    Arena* a = cx->a;
    // `div().id(ix)`: a tab is named by its place in the strip, which the bar
    // above it scopes.
    El* e = Div(a)
                ->PathClick(id)
                ->Role(AccessibilityRole::Tab)
                ->AriaSelected(selected)
                ->AriaDisabled(disabled);
    if (accessibilityLabel.s) {
        e->AriaLabel(accessibilityLabel);
    }
    if (positionInSet > 0) {
        e->AriaPositionInSet(positionInSet);
    }
    if (sizeOfSet > 0) {
        e->AriaSizeOfSet(sizeOfSet);
    }
    if (!disabled && onClick.IsValid()) {
        e->OnClick(onClick);
    }
    return e;
}
} // namespace gpui
