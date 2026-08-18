#include "component/Input.h"
#include "component/Button.h"

namespace gpui {

namespace component {

Input* Input::New(Ctx* cx, Str id, LineInput* state) {
    Arena* a = cx->a;
    Input* i = ArenaNew<Input>(a);
    i->a = a;
    i->cx = cx;
    i->id = id;
    i->state = state;
    return i;
}
Input* Input::Label(Str s) {
    label = s;
    return this;
}
Input* Input::OnChange(Listener fn) {
    onChange = fn;
    return this;
}

Input* Input::OnFocus(Listener fn) {
    onFocus = fn;
    return this;
}

El* Input::IntoEl() {
    const Theme& th = cx->theme();
    El* col = Div(a)->FlexCol()->Gap(4);
    if (label.s) {
        col->Child(TextEl(a, label)->Font(12)->Fg(th.foreground));
    }
    El* field =
        InputBase::New(cx, id, HashClickId(id))
            ->H(28)
            ->PadX(8)
            ->ItemsCenter()
            ->Border(1, state && state->focused ? th.foreground : th.border)
            ->Child(gpui::Input::New(cx, state));
    if (onFocus.IsValid()) {
        field->OnClick(onFocus);
    } else if (onChange.IsValid()) {
        field->OnClick(onChange);
    }
    col->Child(field);
    return col;
}

Textarea* Textarea::New(Ctx* cx, Str id, const char* text) {
    Arena* a = cx->a;
    Textarea* t = ArenaNew<Textarea>(a);
    t->a = a;
    t->cx = cx;
    t->id = id;
    t->text = text;
    return t;
}
Textarea* Textarea::OnFocus(Listener fn) {
    onFocus = fn;
    return this;
}
El* Textarea::IntoEl() {
    const Theme& th = cx->theme();
    El* box = InputBase::New(cx, id, HashClickId(id))
                  ->H(64)
                  ->Pad(8)
                  ->ClipY()
                  ->Border(1, th.border)
                  ->Child(gpui::Textarea::New(cx, text));
    if (onFocus.IsValid()) {
        box->OnClick(onFocus);
    }
    return box;
}

NumberInput* NumberInput::New(Ctx* cx, LineInput* state) {
    Arena* a = cx->a;
    NumberInput* n = ArenaNew<NumberInput>(a);
    n->a = a;
    n->cx = cx;
    n->state = state;
    return n;
}
NumberInput* NumberInput::OnInc(Listener fn) {
    onInc = fn;
    return this;
}
NumberInput* NumberInput::OnDec(Listener fn) {
    onDec = fn;
    return this;
}
El* NumberInput::IntoEl() {
    return gpui::NumberInput::New(cx)
        ->FlexRow()
        ->H(28)
        ->Border(1, cx->theme().border)
        ->Child(InputBase::New(cx, StrL("number"), HashClickId(StrL("number")))
                    ->Grow()
                    ->PadX(8)
                    ->ItemsCenter()
                    ->Child(gpui::Input::New(cx, state)))
        ->Child(Button::New(cx, StrL("inc"))
                    ->Label(StrL("+"))
                    ->Compact()
                    ->OnClick(onInc)
                    ->IntoEl())
        ->Child(Button::New(cx, StrL("dec"))
                    ->Label(StrL("−"))
                    ->Compact()
                    ->OnClick(onDec)
                    ->IntoEl());
}

OtpInput* OtpInput::New(Ctx* cx, const char* value, int len) {
    Arena* a = cx->a;
    OtpInput* o = ArenaNew<OtpInput>(a);
    o->a = a;
    o->cx = cx;
    o->value = value;
    o->len = len;
    return o;
}
OtpInput* OtpInput::OnFocus(Listener fn) {
    onFocus = fn;
    return this;
}
El* OtpInput::IntoEl() {
    const Theme& th = cx->theme();
    El* row =
        gpui::OtpInput::New(cx, HashClickId(StrL("otp")))->FlexRow()->Gap(4);
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
