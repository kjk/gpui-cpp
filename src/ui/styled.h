#ifndef GPUI_SRC_UI_STYLED_H_
#define GPUI_SRC_UI_STYLED_H_
/* UI styled extensions — crates/ui/src/styled.rs. */

#include "base/styled.h"
#include "ui/sizing.h"

namespace gpui::component {

// shadcn/ui's shadow-sm for a control raised inside its container. GPUI's
// blur radius is the gaussian deviation, half the CSS blur radius.
inline El* RaisedShadow(El* element) {
    if (!element) {
        return nullptr;
    }
    Rgba ink = Rgba8(0, 0, 0, 26);
    BoxShadow shadows[] = {
        {0, 1, 1.5f, 0, ink, false},
        {0, 1, 1.f, -1.f, ink, false},
    };
    return element->Shadows(shadows, 2);
}

} // namespace gpui::component
#endif // GPUI_SRC_UI_STYLED_H_
