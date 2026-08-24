/* Shared types for the themed gpui-component façade
   crates/ui/src/sizing.rs */

#include "gpui/gpui.h"
#include "base/lib.h"

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

// Icon::with_size, crates/ui/src/icon.rs: size_3 / size_3p5 / size_4 / size_6.
// Not the control-height scale above — an icon inside a Medium control is 16.
inline float UiIconPx(UiSize s) {
    switch (s) {
        case UiSize::XSmall:
            return 12;
        case UiSize::Small:
            return 14;
        case UiSize::Large:
            return 24;
        default:
            return 16;
    }
}

// Size::table_cell_padding, crates/ui/src/sizing.rs. The table's own cells
// are padded by hand; this is what the loading view measures its rows with,
// which is the one place Rust reads the scale rather than a constant.
inline Edges UiTableCellPadding(UiSize s) {
    // Edges::New is left, right, top, bottom — the shared Rect's field order,
    // which is not the one Rust's Edges<Pixels> lists. Named rather than
    // braced so a reader does not have to remember which.
    switch (s) {
        case UiSize::XSmall:
            return Edges::New(4, 4, 2, 2);
        case UiSize::Small:
            return Edges::New(6, 6, 3, 3);
        case UiSize::Large:
            return Edges::New(12, 12, 8, 8);
        default:
            return Edges::New(8, 8, 4, 4);
    }
}

// Size::table_row_height: 26 / 30 / 32 / 40.
inline float UiTableRowHeight(UiSize s) {
    switch (s) {
        case UiSize::XSmall:
            return 26;
        case UiSize::Small:
            return 30;
        case UiSize::Large:
            return 40;
        default:
            return 32;
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
