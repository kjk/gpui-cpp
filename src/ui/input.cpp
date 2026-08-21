#include "ui/input.h"
#include "ui/button.h"

namespace gpui {

namespace component {

Input* Input::New(Ctx* cx, Str id, InputState* state) {
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
Input* Input::WithSize(UiSize s) {
    size = s;
    return this;
}
Input* Input::Align(InputAlign v) {
    align = v;
    return this;
}
Input* Input::Disabled(bool v) {
    disabled = v;
    return this;
}
Input* Input::Cleanable(bool v) {
    cleanable = v;
    return this;
}
Input* Input::Masked(bool v) {
    masked = v;
    return this;
}
Input* Input::MaskToggle(bool v) {
    maskToggle = v;
    return this;
}
Input* Input::Appearance(bool v) {
    appearance = v;
    return this;
}
Input* Input::FocusRing(bool v) {
    focusRing = v;
    return this;
}
Input* Input::TextColor(Rgba c) {
    textColor = c;
    hasTextColor = true;
    return this;
}
Input* Input::OnClear(Listener fn) {
    onClear = fn;
    return this;
}
Input* Input::OnToggleMask(Listener fn) {
    onToggleMask = fn;
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
    bool focused = state && state->focused && !disabled;
    // input_h / input_px / input_py / input_text_size, by size.
    float h = kInputHeight, padX = kInputPadX, padY = kInputPadY,
          font = kInputTextSize;
    if (size == UiSize::Large) {
        h = 44;
        padX = 12;
        padY = 10;
        font = 16;
    } else if (size == UiSize::Small) {
        h = 24;
        padX = 8;
        padY = 2;
    } else if (size == UiSize::XSmall) {
        h = 20;
        padX = 4;
        padY = 0;
        font = 12;
    }
    InputEditorStyle editor;
    editor.foreground = hasTextColor ? textColor : th.foreground;
    editor.mutedForeground = th.mutedFg;
    editor.caret = th.caret;
    editor.selection = RgbaOpacity(th.selection, 0.4f);
    editor.fontSize = font;
    editor.mask = masked;
    editor.align = align == InputAlign::Center  ? 1
                   : align == InputAlign::Right ? 2
                                                : 0;
    if (disabled) {
        editor.foreground = th.mutedFg;
    }
    El* field = InputBase::New(cx, id, disabled ? 0 : HashClickId(id))
                    ->BindInput(disabled ? nullptr : state)
                    ->FlexRow()
                    ->W(width)
                    ->H(h)
                    ->PadX(padX)
                    ->PadY(padY)
                    ->Gap(kInputGap)
                    ->ItemsCenter();
    // input.rs gates the whole focus appearance on `appearance`: a field with
    // none of its own is one somebody else has framed — a NumberInput's
    // editor, sitting inside a frame that shows the focus for it — and a ring
    // around the editor as well would say it twice.
    field->FocusRing(appearance && focusRing);
    if (appearance) {
        field->Radius(th.radius)
            ->Bg(disabled ? th.muted : th.inputBg)
            // The other half of focus_ring_style: the border takes the ring
            // colour, and the ring itself is painted outside it.
            ->Border(1, focused ? th.ring : th.inputBorder);
        // input.rs: the disabled field is faded as a whole, prefix, suffix
        // and text together.
        if (disabled) {
            field->Opacity(0.5f);
        }
    }
    if (prefix) {
        // `.pl_0()`: the prefix owns the space to the left of the editor.
        field->PadL(0)->Child(prefix);
    }
    bool hasValue = state && InputValue(state).len > 0;
    bool trailing = suffix || (cleanable && hasValue) || maskToggle;
    if (prefix || trailing) {
        field
            ->Child(Div(a)->Grow()->Child(gpui::Input::New(cx, state, editor)));
    } else {
        field->Child(gpui::Input::New(cx, state, editor));
    }
    if (maskToggle) {
        field->Child(Button::New(cx, StrDup(a, fmt("%s-mask", id)))
                         ->Text()
                         ->WithSize(UiSize::XSmall)
                         ->Icon(IconName::Eye)
                         ->TabStop(false)
                         ->OnClick(onToggleMask)
                         ->IntoEl());
    }
    if (cleanable && hasValue && !disabled) {
        // clear_button.rs: `.tab_stop(false)`. The X belongs to the field, and
        // Tab should move to the next field rather than stop at it.
        field->Child(Button::New(cx, StrDup(a, fmt("%s-clean", id)))
                         ->Text()
                         ->WithSize(UiSize::XSmall)
                         ->Icon(IconName::X)
                         ->TabStop(false)
                         ->OnClick(onClear)
                         ->IntoEl());
    }
    if (suffix) {
        field->Child(suffix);
    }
    if (!disabled) {
        if (onFocus.IsValid()) {
            field->OnClick(onFocus);
        } else if (onChange.IsValid()) {
            field->OnClick(onChange);
        }
    }
    col->Child(field);
    return col;
}

Textarea* Textarea::New(Ctx* cx, Str id, InputState* state) {
    Arena* a = cx->a;
    Textarea* t = ArenaNew<Textarea>(a);
    t->a = a;
    t->cx = cx;
    t->id = id;
    t->state = state;
    return t;
}
Textarea* Textarea::Rows(int n) {
    rows = n;
    return this;
}
Textarea* Textarea::H(float px) {
    height = px;
    return this;
}
Textarea* Textarea::SoftWrap(bool v) {
    softWrap = v;
    return this;
}
Textarea* Textarea::OnFocus(Listener fn) {
    onFocus = fn;
    return this;
}

El* Textarea::IntoEl() {
    const Theme& th = cx->theme();
    bool focused = state && state->focused;
    InputEditorStyle editor;
    editor.foreground = th.foreground;
    editor.mutedForeground = th.mutedFg;
    editor.caret = th.caret;
    editor.selection = RgbaOpacity(th.selection, 0.4f);
    editor.fontSize = kInputTextSize;
    if (state) {
        state->softWrap = softWrap;
        if (rows > 0) {
            LayoutModeSetRows(&state->mode, rows);
        }
    }
    // A row is one 1.25rem line box, like the single-line input; the border
    // sits outside the padded content, as in GPUI.
    int shownRows = rows > 0 ? rows : state ? LayoutModeRows(state->mode) : 2;
    float h = height > 0 ? height : (float)shownRows * 20.f + 2 * 8 + 2;
    El* box = InputBase::New(cx, id, HashClickId(id))
                  ->BindInput(state)
                  ->W(kFill)
                  ->H(h)
                  ->Pad(8)
                  ->ClipY()
                  ->Radius(th.radius)
                  ->Bg(th.inputBg)
                  ->Border(1, focused ? th.ring : th.inputBorder)
                  // scroll_handle: the rows slide under the box as the caret
                  // moves, and the wheel moves them too.
                  ->ScrollY(state ? state->scrollY : 0)
                  ->Child(gpui::Textarea::New(cx, state, editor));
    if (onFocus.IsValid()) {
        box->OnClick(onFocus);
    }
    return box;
}

NumberInput* NumberInput::New(Ctx* cx, InputState* state) {
    Arena* a = cx->a;
    NumberInput* n = ArenaNew<NumberInput>(a);
    n->a = a;
    n->cx = cx;
    n->state = state;
    return n;
}
NumberInput* NumberInput::New(Ctx* cx, Str id, InputState* state) {
    NumberInput* n = New(cx, state);
    n->id = id;
    return n;
}
NumberInput* NumberInput::WithSize(UiSize s) {
    size = s;
    return this;
}
NumberInput* NumberInput::Disabled(bool v) {
    disabled = v;
    return this;
}
NumberInput* NumberInput::Appearance(bool v) {
    appearance = v;
    return this;
}
NumberInput* NumberInput::FocusRing(bool v) {
    focusRing = v;
    return this;
}
NumberInput* NumberInput::Suffix(El* el) {
    suffix = el;
    return this;
}
NumberInput* NumberInput::Bg(Background c) {
    bg = c;
    hasBg = true;
    return this;
}
NumberInput* NumberInput::TextColor(Rgba c) {
    textColor = c;
    hasTextColor = true;
    return this;
}
NumberInput* NumberInput::OnFocus(Listener fn) {
    onFocus = fn;
    return this;
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
    const Theme& th = cx->theme();
    float h = 32, btn = 32, font = 14;
    if (size == UiSize::Large) {
        h = 44;
        btn = 32;
        font = 16;
    } else if (size == UiSize::Small) {
        h = 24;
        btn = 24;
    } else if (size == UiSize::XSmall) {
        h = 20;
        btn = 24;
        font = 12;
    }
    Rgba border = disabled ? RgbaOpacity(th.inputBorder, 0.5f) : th.inputBorder;
    // number_input.rs tints the frame on focus, the same as an Input's own
    // border: the editor inside wears no appearance of its own, so the frame
    // is the only thing that can say the editor has the keyboard.
    bool focused = state && state->focused && !disabled;
    El* frame = gpui::NumberInput::New(cx)->FlexRow()->W(width)->H(h);
    if (appearance) {
        frame->Radius(th.radius)
            ->Bg(hasBg ? bg
                       : Background(disabled ? th.tokens.muted.color
                                             : th.inputBg))
            ->Border(1, focused && focusRing ? th.ring : border);
    } else if (hasBg) {
        frame->Radius(th.radius)->Bg(bg);
    }
    // The disabled frame is faded as a whole, the way an Input's is: the
    // value, the two step buttons and the suffix all dim together.
    if (disabled) {
        frame->Opacity(0.5f);
    }
    // The step buttons are transparent until hovered, and fill the frame.
    // `rounded_tl`/`rounded_bl` on the one and `rounded_tr`/`rounded_br` on
    // the other: only the outer corners are rounded, to follow the frame. The
    // border is a hairline inside the frame's own, so the button's radius is
    // the frame's less that.
    float stepR = appearance ? th.radius - 1.f : th.radius;
    if (stepR < 0) {
        stepR = 0;
    }
    Rgba stepFg = disabled ? RgbaOpacity(th.secondaryFg, 0.5f) : th.secondaryFg;
    // Both are gpui_base::Buttons that decline focus: pressing one must leave
    // the editor focused, or the frame's ring flickers on every click. That is
    // what number_input.rs pins with
    // `pressing_a_step_button_never_takes_focus_off_the_editor`.
    Str base = id.s ? id : StrL("number");
    El* dec = gpui::Button::New(cx, StrDup(a, fmt("%s-dec", base)), disabled,
                                onDec, false)
                  ->W(btn)
                  ->H(kFill)
                  ->Corners(stepR, 0, 0, stepR)
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Child(IconEl(a, IconName::Minus, font)->Fg(stepFg));
    El* inc = gpui::Button::New(cx, StrDup(a, fmt("%s-inc", base)), disabled,
                                onInc, false)
                  ->W(btn)
                  ->H(kFill)
                  ->Corners(0, stepR, stepR, 0)
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Child(IconEl(a, IconName::Plus, font)->Fg(stepFg));
    if (!disabled) {
        dec->HoverBg(RgbaOpacity(th.inputBorder, 0.4f));
        inc->HoverBg(RgbaOpacity(th.inputBorder, 0.4f));
    }
    frame->Child(dec);
    // The editor sits between them, centered and without its own frame.
    Input* editor = Input::New(cx, id.s ? id : StrL("number"), state)
                        ->WithSize(size)
                        ->Align(InputAlign::Center)
                        ->Appearance(false)
                        ->Disabled(disabled)
                        ->OnFocus(onFocus);
    if (hasTextColor) {
        editor->TextColor(textColor);
    }
    if (suffix) {
        editor->Suffix(suffix);
    }
    frame->Child(Div(a)->Grow()->H(kFill)->Child(editor->IntoEl()));
    frame->Child(inc);
    return frame;
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
OtpInput* OtpInput::New(Ctx* cx, Str id, Entity<OtpState> state) {
    OtpInput* o = New(cx, nullptr, 0);
    o->id = id;
    o->state = state;
    if (OtpState* s = state.Get(cx)) {
        o->value = s->value;
        o->len = s->len;
        o->slots = s->length;
        o->masked = s->masked;
        o->disabled = s->disabled;
    }
    return o;
}

OtpInput* OtpInput::Id(Str s) {
    id = s;
    return this;
}
OtpInput* OtpInput::Slots(int n) {
    slots = n;
    return this;
}
OtpInput* OtpInput::Groups(int n) {
    groups = n;
    return this;
}
OtpInput* OtpInput::Masked(bool v) {
    masked = v;
    return this;
}
OtpInput* OtpInput::Disabled(bool v) {
    disabled = v;
    return this;
}
OtpInput* OtpInput::FocusRing(bool v) {
    focusRing = v;
    return this;
}
OtpInput* OtpInput::WithSize(UiSize s) {
    size = s;
    return this;
}
OtpInput* OtpInput::CellSize(float px) {
    cellPx = px;
    return this;
}
OtpInput* OtpInput::OnFocus(Listener fn) {
    onFocus = fn;
    return this;
}

El* OtpInput::IntoEl() {
    const Theme& th = cx->theme();
    float cell = 32, text = 16;
    if (cellPx > 0) {
        cell = cellPx;
        text = cellPx * 0.5f;
    } else if (size == UiSize::Large) {
        cell = 44;
        text = 18;
    } else if (size == UiSize::Small || size == UiSize::XSmall) {
        cell = 24;
        text = 14;
    }
    int nGroups = groups < 1 ? 1 : (groups > slots ? slots : groups);
    int per = (slots + nGroups - 1) / nGroups;
    if (per < 1) {
        per = 1;
    }
    Rgba fg = disabled ? th.mutedFg : th.secondaryFg;
    // gap_5 between the groups, gap_1 inside one.
    Str rowId = id.s ? id : StrL("otp");
    El* row = (state.IsValid() ? gpui::OtpInput::New(cx, rowId, state)
                               : gpui::OtpInput::New(cx, rowId))
                  ->FocusRing(focusRing)
                  ->FlexRow()
                  ->ItemsCenter()
                  ->Gap(20);
    if (onFocus.IsValid() && !disabled) {
        row->OnClick(onFocus);
    }
    El* group = nullptr;
    for (int i = 0; i < slots; i++) {
        if (i % per == 0) {
            group = Div(a)->FlexRow()->ItemsCenter()->Gap(4);
            row->Child(group);
        }
        El* box = Div(a)
                      ->W(cell)
                      ->H(cell)
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->Radius(th.radius)
                      ->Bg(disabled ? th.muted : th.inputBg)
                      ->Border(1, th.inputBorder);
        // The caret sits in the first empty cell of a focused field, which is
        // where the next digit lands.
        OtpState* st = state.Get(cx);
        if (st && st->focused && i == st->len && !disabled &&
            OtpCursorVisible(st, cx->app)) {
            box->Border(1, th.ring);
        }
        if (value && i < len) {
            if (masked) {
                box->Child(IconEl(a, IconName::Asterisk, text)->Fg(fg));
            } else {
                char ch[2] = {value[i], 0};
                box->Child(TextEl(a, StrDup(a, Str(ch)))
                               ->Font(text)
                               ->LineHeight(1.f)
                               ->Fg(fg));
            }
        }
        group->Child(box);
    }
    return row;
}

} // namespace component
} // namespace gpui
