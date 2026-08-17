#include "component/Input.h"
#include "component/Button.h"

namespace gpui {

namespace component {

Input* Input::New(Arena* a, Str id, LineInput* state) {
    Input* i = ArenaNew<Input>(a);
    i->a = a;
    i->id = id;
    i->state = state;
    return i;
}
Input* Input::Label(Str s) {
    label = s;
    return this;
}
Input* Input::OnChange(Func0 fn) {
    onChange = fn;
    return this;
}

El* Input::IntoEl() {
    const Theme& th = ThemeNow();
    El* col = Div(a)->FlexCol()->Gap(4);
    if (label.s) {
        col->Child(TextEl(a, label)->Font(12)->Fg(th.foreground));
    }
    El* field =
        InputBase::New(a, id, HashClickId(id))
            ->H(28)
            ->PadX(8)
            ->ItemsCenter()
            ->Border(1, state && state->focused ? th.foreground : th.border)
            ->Child(gpui::Input::New(a, state));
    if (onChange.IsValid()) {
        field->OnClick(onChange);
    }
    col->Child(field);
    return col;
}

Textarea* Textarea::New(Arena* a, Str id, const char* text) {
    Textarea* t = ArenaNew<Textarea>(a);
    t->a = a;
    t->id = id;
    t->text = text;
    return t;
}
Textarea* Textarea::OnFocus(Func0 fn) {
    onFocus = fn;
    return this;
}
El* Textarea::IntoEl() {
    const Theme& th = ThemeNow();
    El* box = InputBase::New(a, id, HashClickId(id))
                  ->H(64)
                  ->Pad(8)
                  ->ClipY()
                  ->Border(1, th.border)
                  ->Child(gpui::Textarea::New(a, text));
    if (onFocus.IsValid()) {
        box->OnClick(onFocus);
    }
    return box;
}

NumberInput* NumberInput::New(Arena* a, LineInput* state) {
    NumberInput* n = ArenaNew<NumberInput>(a);
    n->a = a;
    n->state = state;
    return n;
}
NumberInput* NumberInput::OnInc(Func0 fn) {
    onInc = fn;
    return this;
}
NumberInput* NumberInput::OnDec(Func0 fn) {
    onDec = fn;
    return this;
}
El* NumberInput::IntoEl() {
    return gpui::NumberInput::New(a)
        ->FlexRow()
        ->H(28)
        ->Border(1, ThemeNow().border)
        ->Child(InputBase::New(a, StrL("number"), HashClickId(StrL("number")))
                    ->Grow()
                    ->PadX(8)
                    ->ItemsCenter()
                    ->Child(gpui::Input::New(a, state)))
        ->Child(Button::New(a, StrL("inc"))
                    ->Label(StrL("+"))
                    ->Compact()
                    ->OnClick(onInc)
                    ->IntoEl())
        ->Child(Button::New(a, StrL("dec"))
                    ->Label(StrL("−"))
                    ->Compact()
                    ->OnClick(onDec)
                    ->IntoEl());
}

OtpInput* OtpInput::New(Arena* a, const char* value, int len) {
    OtpInput* o = ArenaNew<OtpInput>(a);
    o->a = a;
    o->value = value;
    o->len = len;
    return o;
}
OtpInput* OtpInput::OnFocus(Func0 fn) {
    onFocus = fn;
    return this;
}
El* OtpInput::IntoEl() {
    const Theme& th = ThemeNow();
    El* row =
        gpui::OtpInput::New(a, HashClickId(StrL("otp")))->FlexRow()->Gap(4);
    if (onFocus.IsValid()) {
        row->OnClick(onFocus);
    }
    for (int i = 0; i < slots; i++) {
        char ch[2] = {' ', 0};
        if (value && i < len) {
            ch[0] = value[i];
        }
        row->Child(Div(a)
                       ->W(28)
                       ->H(28)
                       ->ItemsCenter()
                       ->JustifyCenter()
                       ->Border(1, i == len ? th.foreground : th.border)
                       ->Child(TextEl(a, StrDup(a, Str(ch)))
                                   ->Font(14)
                                   ->Fg(th.foreground)));
    }
    return row;
}

} // namespace component
} // namespace gpui
