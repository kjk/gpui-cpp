#include "base/number_input.h"
#include "base/element_ext.h"

namespace gpui {

El* NumberInput::New(Ctx* cx) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("example-number"), 0);
}
} // namespace gpui
