
#include "gpui/Gpui.h"

namespace gpui {

inline El* UiRoot(Arena* a, Str id, int clickId = 0) {
    El* e = Div(a)->Id(id);
    if (clickId) {
        e->Click(clickId)->FocusId(clickId);
    }
    return e;
}
} // namespace gpui
