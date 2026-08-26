#include "base/radio_group.h"

namespace gpui {

El* RadioGroup::New(Ctx* cx, Str id, Axis axis) {
    Arena* a = cx->a;
    return Div(a)
        ->Id(id)
        ->Role(AccessibilityRole::RadioGroup)
        ->AriaOrientation(axis == Axis::Vertical
                              ? AccessibilityOrientation::Vertical
                              : AccessibilityOrientation::Horizontal);
}
} // namespace gpui
