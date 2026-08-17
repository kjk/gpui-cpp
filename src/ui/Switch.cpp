#include "ui/Switch.h"
#include "ui/Primitive.h"

El* Switch::New(Arena* a, Str id, int clickId) {
    return UiRoot(a, id, clickId);
}

El* SwitchTrack::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}

El* SwitchThumb::New(Arena* a) {
    return Div(a);
}
