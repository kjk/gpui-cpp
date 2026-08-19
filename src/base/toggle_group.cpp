#include "base/toggle_group.h"

namespace gpui {

El* ToggleGroup::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}
} // namespace gpui
