#include "base/toggle_group.h"

namespace gpui {

El* ToggleGroup::New(Ctx* cx, Str id, Axis axis) {
    Arena* a = cx->a;
    return Div(a)
        ->Id(id)
        ->Role(AccessibilityRole::Toolbar)
        ->AriaOrientation(axis == Axis::Vertical
                              ? AccessibilityOrientation::Vertical
                              : AccessibilityOrientation::Horizontal);
}
} // namespace gpui
