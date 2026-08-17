#pragma once

#include "gpui/Gpui.h"

inline El* UiRoot(Arena* a, Str id, int clickId = 0) {
    El* e = Div(a)->Id(id);
    if (clickId) {
        e->Click(clickId)->FocusId(clickId);
    }
    return e;
}
