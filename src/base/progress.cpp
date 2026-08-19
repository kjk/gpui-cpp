#include "base/progress.h"

namespace gpui {

float ProgressClampValue(float value) {
    if (value < 0) {
        return 0;
    }
    return value > 100 ? 100 : value;
}

El* Progress::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
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
