#include "ui/Progress.h"
#include "ui/Primitive.h"

namespace gpui {

El* Progress::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}

El* ProgressTrack::New(Arena* a) {
    return Div(a);
}

El* ProgressIndicator::New(Arena* a) {
    return Div(a);
}
} // namespace gpui
