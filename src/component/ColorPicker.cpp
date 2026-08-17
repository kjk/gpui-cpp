#include "component/ColorPicker.h"

namespace component {

struct ColBind {
    Func1<u32> fn;
    u32 hex = 0;
};
static void FireCol(ColBind* b) {
    b->fn.Call(b->hex);
}

ColorPicker* ColorPicker::New(Arena* a) {
    ColorPicker* c = ::New<ColorPicker>(a);
    c->a = a;
    return c;
}
ColorPicker* ColorPicker::Hex(u32 h) {
    hex = h;
    return this;
}
ColorPicker* ColorPicker::Open(bool v) {
    open = v;
    return this;
}
ColorPicker* ColorPicker::OnChange(Func1<u32> fn) {
    onChange = fn;
    return this;
}
ColorPicker* ColorPicker::OnToggle(Func0 fn) {
    onToggle = fn;
    return this;
}

El* ColorPicker::IntoEl() {
    const Theme& th = ThemeNow();
    Rgba c = Rgb((u8)((hex >> 16) & 0xff), (u8)((hex >> 8) & 0xff),
                 (u8)(hex & 0xff));
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
        static const u32 sw[] = {0xdc2626, 0xd97706, 0x16a34a, 0x2563eb,
                                 0x7c3aed};
        pop = Div(a)
                  ->FlexRow()
                  ->Gap(4)
                  ->Pad(8)
                  ->Border(1, th.foreground)
                  ->Bg(th.background);
        for (int i = 0; i < 5; i++) {
            Rgba sc = Rgb((u8)((sw[i] >> 16) & 0xff), (u8)((sw[i] >> 8) & 0xff),
                          (u8)(sw[i] & 0xff));
            El* cell = ColorSwatch::New(a, StrDup(a, fmt("sw%d", i)))
                           ->W(24)
                           ->H(24)
                           ->Bg(sc);
            if (onChange.IsValid()) {
                ColBind* b = ::New<ColBind>(a);
                b->fn = onChange;
                b->hex = sw[i];
                BindClick(cell, StrDup(a, fmt("sw%d", i)),
                          MkFunc0(&FireCol, b));
            }
            pop->Child(cell);
        }
    }
    El* root = ::ColorPicker::New(a, StrL("color-picker"))->Child(trigger);
    return Popup::New(a, StrL("color-pop"), root)->Content(pop)->IntoEl();
}

} // namespace component
