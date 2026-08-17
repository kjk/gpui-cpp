#include "ui/ColorPicker.h"
#include "ui/Primitive.h"

El* ColorPicker::New(Arena* a, Str id) {
    return UiRoot(a, id, 0);
}
El* ColorSwatch::New(Arena* a, Str id, int clickId) {
    return UiRoot(a, id, clickId);
}
