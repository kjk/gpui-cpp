#include "ui/TextSelection.h"
#include "ui/Primitive.h"

namespace gpui {

El* TextSelection::New(Arena* a, Str id, int clickId) {
    return UiRoot(a, id, clickId);
}
} // namespace gpui
