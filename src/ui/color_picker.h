#ifndef GPUI_UI_COLOR_PICKER_H_
#define GPUI_UI_COLOR_PICKER_H_
/* Themed color picker — crates/ui/src/color_picker.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// Compatibility state lookup for the id-based constructor. The source-shaped
// overload below takes the application-owned Entity<ColorPickerState>
// directly.
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
    // Compatibility callback for the id-based surface. Retained callers
    // subscribe to ColorPickerEvent on `state`.
    Listener onChange;
    Entity<ColorPickerState> state = {};

    static ColorPicker* New(Ctx* cx, Str id);
    static ColorPicker* New(Ctx* cx, Entity<ColorPickerState> state);
    ColorPicker* Label(Str s);
    ColorPicker* Icon(IconName v);
    ColorPicker* WithSize(UiSize s);
    ColorPicker* FeaturedColors(const uint32_t* colors, int n);
    ColorPicker* OnChange(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
#endif // GPUI_UI_COLOR_PICKER_H_
