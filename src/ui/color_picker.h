/* Themed color picker — crates/ui/src/color_picker.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// The state behind a picker. Rust's
// `ColorPicker::new(&Entity<ColorPickerState>)` takes one the application made;
// here the widget keeps it keyed by its id, the way Rating and PopupMenu keep
// theirs, and this is how a view reaches it to seed a value or read the one
// that is committed.
Entity<ColorPickerState> ColorPickerStateFor(Ctx* cx, Str id);

struct ColorPicker {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Str label = {};
    // icon(): the trigger is that icon rather than a square of the value.
    IconName icon = IconName::None;
    UiSize size = UiSize::Medium;
    // featured_colors(): the top row of the palette panel. Null is the
    // theme's own twelve — the six base hues and their light halves.
    const uint32_t* featured = nullptr;
    int nFeatured = 0;
    // ColorPickerEvent::Change, with the committed colour as the argument.
    Listener onChange;

    static ColorPicker* New(Ctx* cx, Str id);
    ColorPicker* Label(Str s);
    ColorPicker* Icon(IconName v);
    ColorPicker* WithSize(UiSize s);
    ColorPicker* FeaturedColors(const uint32_t* colors, int n);
    ColorPicker* OnChange(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
