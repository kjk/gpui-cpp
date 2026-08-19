#include "base/text_selection.h"
#include "base/element_ext.h"

namespace gpui {

El* TextSelection::New(Ctx* cx, Str id, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, id, clickId);
}
} // namespace gpui
