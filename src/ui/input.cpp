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
    // Rust's Input is the field and nothing else — `.flex().size_full()` — and
    // a label above it is the caller's own div. `Input::Label` is this tree's
    // addition, so the column only exists when one was asked for, and it is
    // the column that then carries the width. A field wrapped in a box that
    // does not carry it shrinks to its text the moment it is laid out in a
    // row rather than a column.
    El* col = label.s ? Div(a)->FlexCol()->Gap(4)->W(width) : nullptr;
    if (col) {
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
                    ->W(col ? kFill : width)
                    ->H(h)
                    ->PadX(padX)
                    ->PadY(padY)
                    ->Gap(kInputGap)
                    ->ItemsCenter()
                    // The editor paints its own line into the field's box;
                    // a value wider than the field scrolls under it rather
                    // than spilling out past whatever is next to it.
                    ->ClipX();
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
        field->Child(
            Div(a)->Flex1()->Child(gpui::Input::New(cx, state, editor)));
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
    if (!col) {
        return field;
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
    // `.h(px(..))` or `.h(relative(1.))`: a caller that gives the editor a
    // height means it, and kFill is the relative one — the inspector's pane
    // is what asks for it. Everything else is `rows` line boxes.
    float h = (height > 0 || height == kFill)
                  ? height
                  : (float)shownRows * 20.f + 2 * 8 + 2;
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
    frame->Child(Div(a)->Flex1()->H(kFill)->Child(editor->IntoEl()));
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

// --- SearchPanel, crates/ui/src/input/search.rs ---------------------------

// The bar's own two fields, and what it needs to remember between frames.
// The session itself lives on the field being searched, so closing the bar
// and opening it again keeps the query.
struct SearchPanelState {
    InputState query;
    InputState replacement;
    // The field the bar is over, pointed at as the bar builds. It outlives
    // the frame — a caller's member, or an entity's — so the handlers reach
    // it between frames the way Rust's WeakEntity does.
    InputState* target = nullptr;
    // The box the query field was laid out in, which the replacement field
    // below it is given so the two line up. Rust reads it in on_prepaint,
    // and this is a frame behind for the same reason any laid-out box is.
    Bounds queryBounds = {};
    bool seeded = false;
    // Whether the session was open on the last build, so opening it puts the
    // caret in the query field exactly once.
    bool wasOpen = false;

    static void OnQueryEvent(SearchPanelState* self, Ctx* cx,
                             const InputEvent* ev);
    static void OnReplacementEvent(SearchPanelState* self, Ctx* cx,
                                   const InputEvent* ev);
    static void OnToggleCase(SearchPanelState* self, Ctx* cx,
                             const ClickEvent*);
    static void OnToggleReplace(SearchPanelState* self, Ctx* cx,
                                const ClickEvent*);
    static void OnPrev(SearchPanelState* self, Ctx* cx, const ClickEvent*);
    static void OnNext(SearchPanelState* self, Ctx* cx, const ClickEvent*);
    static void OnClose(SearchPanelState* self, Ctx* cx, const ClickEvent*);
    static void OnReplaceOne(SearchPanelState* self, Ctx* cx,
                             const ClickEvent*);
    static void OnReplaceAll(SearchPanelState* self, Ctx* cx,
                             const ClickEvent*);
    static void OnKey(SearchPanelState* self, Ctx* cx, const KeyEvent* ev);
};

// The bar closes and the field it was over takes the caret back, which is
// what makes escape put you where you were.
static void PanelClose(SearchPanelState* self, Ctx* cx) {
    if (!self->target) {
        return;
    }
    InputCloseSearch(self->target, cx->app, cx->win);
    InputFocus(self->target, cx->app, cx->win);
}

void SearchPanelState::OnQueryEvent(SearchPanelState* self, Ctx* cx,
                                    const InputEvent* ev) {
    if (!self->target) {
        return;
    }
    if (ev->kind == InputEventKind::Change) {
        InputSetSearchQuery(self->target, cx->app, cx->win,
                            InputValue(&self->query),
                            self->target->search.caseInsensitive);
        return;
    }
    if (ev->kind == InputEventKind::PressEnter) {
        // on_action_enter: enter walks forward, shift-enter back.
        if (ev->shift) {
            InputSearchPrev(self->target, cx->app, cx->win, nullptr);
        } else {
            InputSearchNext(self->target, cx->app, cx->win, nullptr);
        }
    }
}

void SearchPanelState::OnReplacementEvent(SearchPanelState* self, Ctx* cx,
                                          const InputEvent* ev) {
    if (!self->target) {
        return;
    }
    if (ev->kind == InputEventKind::Change) {
        SearchSessionSetReplacement(&self->target->search,
                                    InputValue(&self->replacement));
        Notify(cx);
        return;
    }
    if (ev->kind == InputEventKind::PressEnter) {
        InputSearchReplaceOne(self->target, cx->app, cx->win,
                              InputValue(&self->replacement));
    }
}

void SearchPanelState::OnToggleCase(SearchPanelState* self, Ctx* cx,
                                    const ClickEvent*) {
    if (!self->target) {
        return;
    }
    SearchSession* ss = &self->target->search;
    InputSetSearchQuery(self->target, cx->app, cx->win,
                        InputValue(&self->query), !ss->caseInsensitive);
}

void SearchPanelState::OnToggleReplace(SearchPanelState* self, Ctx* cx,
                                       const ClickEvent*) {
    if (self->target) {
        InputSetSearchReplaceMode(self->target, cx->app, cx->win,
                                  !self->target->search.replaceMode);
    }
}

void SearchPanelState::OnPrev(SearchPanelState* self, Ctx* cx,
                              const ClickEvent*) {
    if (self->target) {
        InputSearchPrev(self->target, cx->app, cx->win, nullptr);
    }
}

void SearchPanelState::OnNext(SearchPanelState* self, Ctx* cx,
                              const ClickEvent*) {
    if (self->target) {
        InputSearchNext(self->target, cx->app, cx->win, nullptr);
    }
}

void SearchPanelState::OnClose(SearchPanelState* self, Ctx* cx,
                               const ClickEvent*) {
    PanelClose(self, cx);
}

void SearchPanelState::OnReplaceOne(SearchPanelState* self, Ctx* cx,
                                    const ClickEvent*) {
    if (self->target) {
        InputSearchReplaceOne(self->target, cx->app, cx->win,
                              InputValue(&self->replacement));
    }
}

void SearchPanelState::OnReplaceAll(SearchPanelState* self, Ctx* cx,
                                    const ClickEvent*) {
    if (self->target) {
        InputSearchReplaceAll(self->target, cx->app, cx->win,
                              InputValue(&self->replacement));
    }
}

void SearchPanelState::OnKey(SearchPanelState* self, Ctx* cx,
                             const KeyEvent* ev) {
    if (!ev->down || !self->target) {
        return;
    }
    if (ev->vk == KeyEscape) {
        // on_action_escape.
        PanelClose(self, cx);
        WindowStopPropagation(cx);
        return;
    }
    if (ev->vk == KeyTab && self->target->search.replaceMode) {
        // on_action_tab / on_action_tab_prev: the two fields, and nothing
        // outside them — the focus ring never sees this tab.
        InputState* to =
            self->query.focused ? &self->replacement : &self->query;
        InputFocus(to, cx->app, cx->win);
        InputSelectAll(to, cx->app, cx->win);
        WindowStopPropagation(cx);
    }
}

SearchPanel* SearchPanel::New(Ctx* cx, Str id, InputState* target) {
    Arena* a = cx->a;
    SearchPanel* p = ArenaNew<SearchPanel>(a);
    p->a = a;
    p->cx = cx;
    p->id = id;
    p->target = target;
    return p;
}

El* SearchPanel::IntoEl() {
    const Theme& th = cx->theme();
    // The state is the window's, keyed by the bar's id, so two editors on one
    // page each get their own.
    Entity<SearchPanelState> ent = KeyedEntity<SearchPanelState>(
        cx, KeyedKey(HashClickId(id), HashClickId(StrL("search-panel"))));
    SearchPanelState* st = ent.Get(cx);
    if (!st || !target) {
        return Div(a);
    }
    st->target = target;
    if (!st->seeded) {
        st->seeded = true;
        st->query.onChange = ListenTo(ent, &SearchPanelState::OnQueryEvent);
        st->replacement
            .onChange = ListenTo(ent, &SearchPanelState::OnReplacementEvent);
    }
    SearchSession* ss = &target->search;
    if (!ss->open) {
        st->wasOpen = false;
        return Div(a);
    }
    // Opening it puts the query in the field and picks it out, which is what
    // makes typing over it the next search rather than an edit of the last.
    if (!st->wasOpen) {
        st->wasOpen = true;
        InputSetValue(&st->query, ss->query);
        InputFocus(&st->query, cx->app, cx->win);
        InputSelectAll(&st->query, cx->app, cx->win);
    }
    bool hasMatches = !SearchMatcherIsEmpty(&ss->matcher);
    bool allowReplace = InputIsReplaceable(target);
    if (!allowReplace) {
        ss->replaceMode = false;
    }

    El* panel = Div(a)
                    ->FlexCol()
                    ->W(kFill)
                    ->PadY(8)
                    ->PadX(12)
                    ->Gap(4)
                    ->Bg(th.tokens.background)
                    ->BorderB(1, th.border)
                    ->Radius(th.radius * 0.5f)
                    ->OnKeyDown(ListenTo(ent, &SearchPanelState::OnKey));

    El* row = Div(a)->FlexRow()->W(kFill)->Gap(8)->ItemsCenter();
    El* caseBtn = Button::New(cx, StrDup(a, fmt("%s-case", id)))
                      ->Text()
                      ->Compact()
                      ->WithSize(UiSize::XSmall)
                      ->Icon(IconName::CaseSensitive)
                      ->Selected(!ss->caseInsensitive)
                      ->OnClick(ListenTo(ent, &SearchPanelState::OnToggleCase))
                      ->IntoEl();
    El* queryBox = Div(a)->FlexRow()->Flex1()->Gap(4);
    queryBox->Child(Input::New(cx, StrDup(a, fmt("%s-q", id)), &st->query)
                        ->WithSize(UiSize::Small)
                        ->FocusRing(false)
                        ->Suffix(caseBtn)
                        ->IntoEl()
                        ->BoundsOut(&st->queryBounds));
    row->Child(queryBox);
    if (allowReplace) {
        row->Child(
            Button::New(cx, StrDup(a, fmt("%s-mode", id)))
                ->Ghost()
                ->WithSize(UiSize::XSmall)
                ->Icon(IconName::Replace)
                ->Selected(ss->replaceMode)
                ->OnClick(ListenTo(ent, &SearchPanelState::OnToggleReplace))
                ->IntoEl());
    }
    row->Child(Button::New(cx, StrDup(a, fmt("%s-prev", id)))
                   ->Ghost()
                   ->WithSize(UiSize::XSmall)
                   ->Icon(IconName::ChevronLeft)
                   ->Disabled(!hasMatches)
                   ->OnClick(ListenTo(ent, &SearchPanelState::OnPrev))
                   ->IntoEl());
    row->Child(Button::New(cx, StrDup(a, fmt("%s-next", id)))
                   ->Ghost()
                   ->WithSize(UiSize::XSmall)
                   ->Icon(IconName::ChevronRight)
                   ->Disabled(!hasMatches)
                   ->OnClick(ListenTo(ent, &SearchPanelState::OnNext))
                   ->IntoEl());
    row->Child(TextEl(a, SearchMatcherLabel(a, &ss->matcher))
                   ->Font(14)
                   ->MinW(64)
                   ->Fg(hasMatches ? th.foreground : th.mutedFg));
    // div().w_7(): the gap that keeps the close button off the counter.
    row->Child(Div(a)->W(28));
    row->Child(Button::New(cx, StrDup(a, fmt("%s-close", id)))
                   ->Ghost()
                   ->WithSize(UiSize::XSmall)
                   ->Icon(IconName::WindowClose)
                   ->OnClick(ListenTo(ent, &SearchPanelState::OnClose))
                   ->IntoEl());
    panel->Child(row);

    if (ss->replaceMode && allowReplace) {
        El* row2 = Div(a)->FlexRow()->W(kFill)->Gap(8)->ItemsCenter();
        // The replacement field is as wide as the query field above it,
        // which is what Rust's `input_width` is for. Zero on the first frame,
        // before the query field has been laid out; growing stands in.
        float w = st->queryBounds.w > 1 ? st->queryBounds.w : kFill;
        El* rep = Input::New(cx, StrDup(a, fmt("%s-r", id)), &st->replacement)
                      ->WithSize(UiSize::Small)
                      ->FocusRing(false)
                      ->W(w)
                      ->IntoEl();
        row2->Child(rep);
        row2->Child(
            Button::New(cx, StrDup(a, fmt("%s-rep1", id)))
                ->WithSize(UiSize::Small)
                ->Label(StrL("Replace"))
                ->Disabled(!hasMatches)
                ->OnClick(ListenTo(ent, &SearchPanelState::OnReplaceOne))
                ->IntoEl());
        row2->Child(
            Button::New(cx, StrDup(a, fmt("%s-repall", id)))
                ->WithSize(UiSize::Small)
                ->Label(StrL("Replace All"))
                ->Disabled(!hasMatches)
                ->OnClick(ListenTo(ent, &SearchPanelState::OnReplaceAll))
                ->IntoEl());
        panel->Child(row2);
    }
    return panel;
}

} // namespace component
} // namespace gpui
