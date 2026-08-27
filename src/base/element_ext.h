#ifndef GPUI_BASE_ELEMENT_EXT_H_
#define GPUI_BASE_ELEMENT_EXT_H_

#include "gpui/gpui.h"

namespace gpui {

inline El* UiRoot(Arena* a, Str id, int clickId = 0) {
    El* e = Div(a)->Id(id);
    if (clickId) {
        e->Click(clickId)->FocusId(clickId);
    }
    return e;
}
} // namespace gpui
#endif // GPUI_BASE_ELEMENT_EXT_H_
