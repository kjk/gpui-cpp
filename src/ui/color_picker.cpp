#include "ui/color_picker.h"

namespace gpui {

namespace component {

ColorPicker* ColorPicker::New(Ctx* cx) {
    Arena* a = cx->a;
    ColorPicker* c = ArenaNew<ColorPicker>(a);
    c->a = a;
    c->cx = cx;
    return c;
}
ColorPicker* ColorPicker::Hex(uint32_t h) {
    hex = h;
    hasValue = true;
    return this;
}
ColorPicker* ColorPicker::Label(Str s) {
    label = s;
    return this;
}
ColorPicker* ColorPicker::WithSize(UiSize s) {
    size = s;
    return this;
}
ColorPicker* ColorPicker::Open(bool v) {
    open = v;
    return this;
}
ColorPicker* ColorPicker::OnChange(Listener fn) {
    onChange = fn;
    return this;
}
ColorPicker* ColorPicker::OnToggle(Listener fn) {
    onToggle = fn;
    return this;
}

El* ColorPicker::IntoEl() {
    const Theme& th = cx->theme();
    Rgba c = Rgb((uint8_t)((hex >> 16) & 0xff), (uint8_t)((hex >> 8) & 0xff),
                 (uint8_t)(hex & 0xff));
    // size_with(size): the swatch is the whole trigger; the hex only shows in
    // the tooltip in Rust, so it is not drawn here.
    float sq = 32;
    if (size == UiSize::Large) {
        sq = 44;
    } else if (size == UiSize::Small) {
        sq = 20;
    } else if (size == UiSize::XSmall) {
        sq = 16;
    }
    El* swatch = Div(a)
                     ->W(sq)
                     ->H(sq)
                     ->Radius(th.radius)
                     ->Bg(hasValue ? c : th.background)
                     // darken(0.3) on the value, the input border when empty.
                     ->Border(1, hasValue ? RgbaMix(c, Rgb(0, 0, 0), 0.3f)
                                          : th.inputBorder);
    El* trigger = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->Child(swatch);
    if (label.s) {
        trigger->Child(TextEl(a, label)->Font(16)->Fg(th.foreground));
    }
    BindClick(trigger, StrL("color-trigger"), onToggle);
    El* pop = nullptr;
    if (open) {
        static const uint32_t sw[] = {0xdc2626, 0xd97706, 0x16a34a, 0x2563eb,
                                      0x7c3aed};
        pop = Div(a)
                  ->FlexRow()
                  ->Gap(4)
                  ->Pad(8)
                  ->Border(1, th.foreground)
                  ->Bg(th.background);
        for (int i = 0; i < 5; i++) {
            Rgba sc =
                Rgb((uint8_t)((sw[i] >> 16) & 0xff),
                    (uint8_t)((sw[i] >> 8) & 0xff), (uint8_t)(sw[i] & 0xff));
            El* cell = ColorSwatch::New(cx, StrDup(a, fmt("sw%d", i)))
                           ->W(24)
                           ->H(24)
                           ->Bg(sc);
            if (onChange.IsValid()) {
                BindClick(cell, StrDup(a, fmt("sw%d", i)),
                          ListenerArg(onChange, (intptr_t)sw[i]));
            }
            pop->Child(cell);
        }
    }
    El* root = gpui::ColorPicker::New(cx, StrL("color-picker"))->Child(trigger);
    return Popup::New(cx, StrL("color-pop"), root)->Content(pop)->IntoEl();
}

} // namespace component
} // namespace gpui
