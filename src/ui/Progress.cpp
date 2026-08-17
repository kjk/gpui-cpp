#include "ui/Progress.h"
#include "ui/Primitive.h"

El* Progress::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}

El* ProgressTrack::New(Arena* a) {
    return Div(a);
}

El* ProgressIndicator::New(Arena* a) {
    return Div(a);
}
