#include "ui/TextSelection.h"
#include "ui/Primitive.h"

El* TextSelection::New(Arena* a, Str id, int clickId) {
    return UiRoot(a, id, clickId);
}
