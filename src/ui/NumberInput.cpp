#include "ui/NumberInput.h"
#include "ui/Primitive.h"

El* NumberInput::New(Arena* a) {
    return UiRoot(a, StrL("example-number"), 0);
}
