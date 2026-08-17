#include "component/ColorPicker.h"

namespace gpui {

namespace component {

struct ColBind {
    Func1<uint32_t> fn;
    uint32_t hex = 0;
};
static void FireCol(ColBind* b) {
    b->fn.Call(b->hex);
}

ColorPicker* ColorPicker::New(Ctx* cx) {
    Arena* a = cx->a;
    ColorPicker* c = ArenaNew<ColorPicker>(a);
    c->a = a;
    c->cx = cx;
    return c;
}
ColorPicker* ColorPicker::Hex(uint32_t h) {
    hex = h;
    return this;
}
ColorPicker* ColorPicker::Open(bool v) {
    open = v;
    return this;
}
ColorPicker* ColorPicker::OnChange(Func1<uint32_t> fn) {
    onChange = fn;
    return this;
}
ColorPicker* ColorPicker::OnToggle(Func0 fn) {
    onToggle = fn;
    return this;
}

El* ColorPicker::IntoEl() {
    const Theme& th = ThemeNow();
    Rgba c = Rgb((uint8_t)((hex >> 16) & 0xff), (uint8_t)((hex >> 8) & 0xff),
                 (uint8_t)(hex & 0xff));
    El* trigger =
        Div(a)
            ->H(28)
            ->PadX(8)
            ->ItemsCenter()
            ->Gap(8)
            ->Border(1, th.foreground)
            ->Child(Div(a)->W(14)->H(14)->Bg(c)->Border(1, th.foreground))
            ->Child(TextEl(a, StrDup(a, fmt("#%06x", hex & 0xffffff)))
                        ->Font(12)
                        ->Fg(th.foreground));
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
                ColBind* b = ArenaNew<ColBind>(a);
                b->fn = onChange;
                b->hex = sw[i];
                BindClick(cell, StrDup(a, fmt("sw%d", i)),
                          MkFunc0(&FireCol, b));
            }
            pop->Child(cell);
        }
    }
    El* root = gpui::ColorPicker::New(cx, StrL("color-picker"))->Child(trigger);
    return Popup::New(cx, StrL("color-pop"), root)->Content(pop)->IntoEl();
}

} // namespace component
} // namespace gpui
