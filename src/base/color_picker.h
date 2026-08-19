/* Unstyled color picker — crates/base/src/color_picker.rs */

#include "gpui/gpui.h"

namespace gpui {

// Rust's ColorPickerState holds the committed color *and* a transient preview
// beside it, and that pair is the module's whole interaction model: hovering a
// swatch previews the color, leaving restores what was committed, and only a
// click commits. Everything a picker shows reads the preview when there is one
// and the value otherwise.
//
// The rest of the state is an open flag, an active tab, a hex field and four
// HSLA sliders — and those sliders are SliderStates, which are already ported,
// so a picker builds them rather than this doing it again.
struct ColorPickerState {
    uint32_t value = 0;
    bool hasValue = false;
    uint32_t preview = 0;
    bool hasPreview = false;
    bool open = false;
    int activeTab = 0;
};

// What the picker shows: preview_color while one is up, the committed value
// otherwise. Answers false when there is neither, which is Rust's `None` and
// draws as the empty square.
bool ColorPickerShown(const ColorPickerState* s, uint32_t* out);

// preview_color / clear_preview. Rust's clear is a no-op when the preview is
// already the committed color, so a pointer crossing the swatch that is
// already picked does not repaint anything.
void ColorPickerPreview(ColorPickerState* s, uint32_t color);
bool ColorPickerClearPreview(ColorPickerState* s);

// set_value / clear_value: replace the committed color without emitting a
// change. Committing drops the preview, since what was transient is now what
// the picker holds.
void ColorPickerSetValue(ColorPickerState* s, uint32_t color);
void ColorPickerClearValue(ColorPickerState* s);

struct ColorPicker {
    static El* New(Ctx* cx, Str id);
};
// A swatch previews on hover and commits on click, so it takes both.
struct ColorSwatch {
    static El* New(Ctx* cx, Str id, Listener onClick = {},
                   Listener onHover = {});
};
} // namespace gpui
