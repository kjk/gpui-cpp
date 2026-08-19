#include "base/progress.h"
#include "base/element_ext.h"

namespace gpui {

El* Progress::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return UiRoot(a, id, 0);
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
