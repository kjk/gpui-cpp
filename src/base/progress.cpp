#include "base/progress.h"

namespace gpui {

float ProgressClampValue(float value) {
    if (value < 0) {
        return 0;
    }
    return value > 100 ? 100 : value;
}

El* Progress::New(Ctx* cx, Str id, float value, bool indeterminate,
                  Str accessibilityLabel) {
    Arena* a = cx->a;
    El* e = Div(a)
                ->Id(id)
                ->Role(AccessibilityRole::ProgressIndicator)
                ->AriaMinNumericValue(0)
                ->AriaMaxNumericValue(100);
    if (!indeterminate) {
        e->AriaNumericValue(ProgressClampValue(value));
    }
    if (accessibilityLabel.s) {
        e->AriaLabel(accessibilityLabel);
    }
    return e;
}

El* ProgressTrack::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}

El* ProgressIndicator::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}
} // namespace gpui
