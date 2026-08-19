#include "base/combobox.h"

namespace gpui {

El* Combobox::New(Ctx* cx, Str id) {
    return Select::New(cx, id);
}
} // namespace gpui
