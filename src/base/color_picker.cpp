#include "base/color_picker.h"

namespace gpui {

bool ColorPickerShown(const ColorPickerState* s, uint32_t* out) {
    if (s->hasPreview) {
        *out = s->preview;
        return true;
    }
    if (s->hasValue) {
        *out = s->value;
        return true;
    }
    return false;
}

void ColorPickerPreview(ColorPickerState* s, uint32_t color) {
    s->preview = color;
    s->hasPreview = true;
}

bool ColorPickerClearPreview(ColorPickerState* s) {
    if (!s->hasPreview) {
        return false;
    }
    // Rust returns early when the preview already equals the value: there is
    // nothing to restore, so nothing to repaint.
    if (s->hasValue && s->preview == s->value) {
        s->hasPreview = false;
        return false;
    }
    s->hasPreview = false;
    return true;
}

void ColorPickerSetValue(ColorPickerState* s, uint32_t color) {
    s->value = color;
    s->hasValue = true;
    s->hasPreview = false;
}

void ColorPickerClearValue(ColorPickerState* s) {
    s->hasValue = false;
    s->hasPreview = false;
}

El* ColorPicker::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    return Div(a)->Id(id);
}

El* ColorSwatch::New(Ctx* cx, Str id, Listener onClick, Listener onHover) {
    Arena* a = cx->a;
    El* e = Div(a)->Id(id)->Click(HashClickId(id));
    if (onClick.IsValid()) {
        e->OnClick(onClick);
    }
    if (onHover.IsValid()) {
        e->OnHover(onHover);
    }
    return e;
}
} // namespace gpui
