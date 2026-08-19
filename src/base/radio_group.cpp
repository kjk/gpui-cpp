#include "base/radio_group.h"

namespace gpui {

El* RadioGroup::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}
} // namespace gpui
