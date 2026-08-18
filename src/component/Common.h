/* Shared types for the themed gpui-component façade (crates/ui). */

#include "gpui/Gpui.h"
#include "ui/Ui.h"

namespace gpui {

enum class UiSize : uint8_t {
    XSmall,
    Small,
    Medium,
    Large
};

inline float UiSizePx(UiSize s) {
    switch (s) {
        case UiSize::XSmall:
            return 20;
        case UiSize::Small:
            return 24;
        case UiSize::Large:
            return 36;
        default:
            return 28;
    }
}

inline float UiFontPx(UiSize s) {
    switch (s) {
        case UiSize::XSmall:
            return 11;
        case UiSize::Small:
            return 12;
        case UiSize::Large:
            return 16;
        default:
            return 14;
    }
}

namespace component {

inline El* BindClick(El* e, Str id, Listener onClick) {
    int cid = HashClickId(id);
    e->Id(id)->Click(cid)->FocusId(cid);
    if (onClick.IsValid()) {
        e->OnClick(onClick);
    }
    return e;
}

} // namespace component
} // namespace gpui
