#include "ui/inspector.h"
#include "ui/button.h"
#include "ui/description_list.h"
#include "ui/title_bar.h"

namespace gpui {

namespace component {

Inspector* Inspector::New(Ctx* cx) {
    Arena* a = cx->a;
    Inspector* i = ArenaNew<Inspector>(a);
    i->a = a;
    i->cx = cx;
    return i;
}
Inspector* Inspector::W(float v) {
    width = v;
    return this;
}

static void InspectorPickClick(void*, Ctx* cx, const ClickEvent*) {
    const InspectorState* st = WindowInspector(cx);
    WindowInspectorPick(cx->win, !(st && st->picking));
}

static void InspectorCloseClick(void*, Ctx* cx, const ClickEvent*) {
    WindowToggleInspector(cx->win);
}

static Str KindName(int kind) {
    switch ((ElKind)kind) {
        case ElKind::Text:
            return StrL("Text");
        case ElKind::Chart:
            return StrL("Chart");
        case ElKind::Progress:
            return StrL("Progress");
        case ElKind::Icon:
            return StrL("Icon");
        default:
            return StrL("Div");
    }
}

El* Inspector::IntoEl() {
    const Theme& th = cx->theme();
    const InspectorState* st = WindowInspector(cx);
    if (!st || !st->on) {
        return nullptr;
    }
    El* panel = Div(a)
                    ->FlexCol()
                    ->W(width)
                    ->H(kFill)
                    ->Bg(th.background)
                    ->BorderL(1, th.border);

    // The title bar: the magnifier that starts picking, the name, and the
    // close button — the three things Rust puts there.
    El* bar = Div(a)
                  ->FlexRow()
                  ->W(kFill)
                  ->H(kTitleBarHeight)
                  ->PadX(8)
                  ->Gap(8)
                  ->ItemsCenter()
                  ->JustifyBetween()
                  ->Bg(th.titleBar)
                  ->BorderB(1, th.titleBarBorder);
    El* left = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    left->Child(Button::New(cx, StrL("inspect"))
                    ->Icon(IconName::Search)
                    ->Ghost()
                    ->Selected(st->picking)
                    ->OnClick(Listen(cx, &InspectorPickClick))
                    ->IntoEl());
    left->Child(TextEl(a, StrL("Inspector"))->Font(14)->Fg(th.foreground));
    bar->Child(left);
    bar->Child(Button::New(cx, StrL("inspector-close"))
                   ->Icon(IconName::X)
                   ->Ghost()
                   ->OnClick(Listen(cx, &InspectorCloseClick))
                   ->IntoEl());
    panel->Child(bar);

    El* body = Div(a)->FlexCol()->Grow()->W(kFill)->Pad(12)->Gap(12);
    if (!st->hasPick) {
        body->Child(TextEl(a, StrL("Pick an element to inspect it."))
                        ->Font(13)
                        ->Fg(th.mutedFg));
        panel->Child(body);
        return panel;
    }

    const InspectorPick& p = st->pick;
    // Rust leads with the element's source location; there is none here, so
    // the element leads with what it is and which id it answers to.
    body->Child(TextEl(a, KindName(p.kind))->Font(14)->Fg(th.foreground));
    DescriptionList* dl = DescriptionList::New(cx)->Columns(1);
    if (p.elId.s) {
        dl->Item(StrL("id"), p.elId);
    }
    if (p.id) {
        dl->Item(StrL("click id"), StrDup(a, fmt("%d", p.id)));
    }
    dl->Item(StrL("origin"),
             StrDup(a, fmt("%d, %d", (int)p.bounds.x, (int)p.bounds.y)));
    dl->Item(StrL("size"),
             StrDup(a, fmt("%d × %d", (int)p.bounds.w, (int)p.bounds.h)));
    dl->Item(StrL("depth"), StrDup(a, fmt("%d", p.depth)));
    dl->Item(StrL("direction"), p.row ? StrL("row") : StrL("column"));
    if (p.pad > 0) {
        dl->Item(StrL("padding"), StrDup(a, fmt("%d", (int)p.pad)));
    }
    if (p.gap > 0) {
        dl->Item(StrL("gap"), StrDup(a, fmt("%d", (int)p.gap)));
    }
    if (p.radius > 0) {
        dl->Item(StrL("radius"), StrDup(a, fmt("%d", (int)p.radius)));
    }
    if (p.border > 0) {
        dl->Item(StrL("border"), StrDup(a, fmt("%d", (int)p.border)));
    }
    if (p.font > 0) {
        dl->Item(StrL("font size"), StrDup(a, fmt("%d", (int)p.font)));
    }
    if (p.hasBg) {
        dl->Item(StrL("background"),
                 StrDup(a, fmt("#%02x%02x%02x", p.bg.r, p.bg.g, p.bg.b)));
    }
    if (p.text.s && p.text.len > 0) {
        dl->Item(StrL("text"), p.text);
    }
    body->Child(dl->IntoEl());
    panel->Child(body);
    return panel;
}

} // namespace component
} // namespace gpui
