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
Input* Input::Prefix(El* el) {
    prefix = el;
    return this;
}
Input* Input::Suffix(El* el) {
    suffix = el;
    return this;
}
Input* Input::W(float v) {
    width = v;
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

// Size::Medium, the default: input_h is h_8, input_px 10, input_py 8,
// input_text_size text_sm, gap 6 (crates/ui/src/sizing.rs).
static const float kInputHeight = 32;
static const float kInputPadX = 10;
static const float kInputPadY = 8;
static const float kInputGap = 6;
static const float kInputTextSize = 14;

El* Input::IntoEl() {
    const Theme& th = cx->theme();
    El* col = Div(a)->FlexCol()->Gap(4);
    if (label.s) {
        col->Child(TextEl(a, label)->Font(12)->Fg(th.foreground));
    }
    bool focused = state && state->focused;
    InputEditorStyle editor;
    editor.foreground = th.foreground;
    editor.mutedForeground = th.mutedFg;
    editor.caret = th.caret;
    editor.fontSize = kInputTextSize;
    El* field =
        InputBase::New(cx, id, HashClickId(id))
            ->FlexRow()
            ->W(width)
            ->H(kInputHeight)
            ->PadX(kInputPadX)
            ->PadY(kInputPadY)
            ->Gap(kInputGap)
            ->ItemsCenter()
            ->Radius(th.radius)
            ->Bg(th.inputBg)
            // Rust draws a ring outside the border box on focus; without one
            // here the ring color goes on the border itself.
            ->Border(1, focused ? th.ring : th.inputBorder);
    if (prefix) {
        // `.pl_0()`: the prefix owns the space to the left of the editor.
        field->PadL(0)->Child(prefix);
    }
    if (prefix || suffix) {
        field
            ->Child(Div(a)->Grow()->Child(gpui::Input::New(cx, state, editor)));
    } else {
        field->Child(gpui::Input::New(cx, state, editor));
    }
    if (suffix) {
        field->Child(suffix);
    }
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
Textarea* Textarea::Rows(int n) {
    rows = n;
    return this;
}
Textarea* Textarea::OnFocus(Listener fn) {
    onFocus = fn;
    return this;
}
El* Textarea::IntoEl() {
    const Theme& th = cx->theme();
    InputEditorStyle editor;
    editor.foreground = th.foreground;
    editor.mutedForeground = th.mutedFg;
    editor.caret = th.caret;
    editor.fontSize = kInputTextSize;
    // A row is one 1.25rem line box, like the single-line input; the border
    // sits outside the padded content, as in GPUI.
    float h = rows > 0 ? (float)rows * 20.f + 2 * 8 + 2 : 64;
    El* box = InputBase::New(cx, id, HashClickId(id))
                  ->W(kFill)
                  ->H(h)
                  ->Pad(8)
                  ->ClipY()
                  ->Radius(th.radius)
                  ->Bg(th.inputBg)
                  ->Border(1, th.inputBorder)
                  ->Child(gpui::Textarea::New(cx, text, editor));
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
NumberInput* NumberInput::W(float v) {
    width = v;
    return this;
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
        ->W(width)
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
