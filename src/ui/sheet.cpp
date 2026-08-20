#include "ui/sheet.h"
#include "base/actions.h"
#include "base/motion.h"
#include "ui/button.h"

namespace gpui {

namespace component {

// sheet.rs: the surface takes 0.15 s to slide in.
static const float kSheetMotionMs = 150.f;

Sheet* Sheet::New(Ctx* cx) {
    Arena* a = cx->a;
    Sheet* s = ArenaNew<Sheet>(a);
    s->a = a;
    s->cx = cx;
    return s;
}
Sheet* Sheet::Title(Str s) {
    title = s;
    return this;
}
Sheet* Sheet::Placement(SheetPlacement p) {
    placement = p;
    return this;
}
Sheet* Sheet::Size(float px) {
    size = px;
    return this;
}
Sheet* Sheet::Overlay(bool v) {
    overlay = v;
    return this;
}
Sheet* Sheet::Open(bool v) {
    open = v;
    return this;
}
Sheet* Sheet::Body(El* e) {
    body = e;
    return this;
}
Sheet* Sheet::OnClose(Listener fn) {
    onClose = fn;
    return this;
}

El* Sheet::IntoEl(WinSize win) {
    if (!open) {
        return Div(a);
    }
    const Theme& th = cx->theme();
    bool horizontal =
        placement == SheetPlacement::Left || placement == SheetPlacement::Right;
    El* surface = Div(a)
                      ->Absolute()
                      ->W(horizontal ? size : win.dipW)
                      ->H(horizontal ? win.dipH : size)
                      ->Pad(16)
                      ->FlexCol()
                      ->Gap(12)
                      ->Bg(th.background)
                      ->Border(1, th.border);
    // sheet.rs's "slide": 0.15 s from a hundred pixels off its own edge to
    // where it belongs. GPUI's default easing is linear, and the offset is
    // whichever way the sheet came in from.
    float delta = MotionAppear(cx, MotionId(StrL("sheet")), kSheetMotionMs);
    float off = -100.f + delta * 100.f;
    switch (placement) {
        case SheetPlacement::Left:
            surface->Top(0)->Left(off);
            break;
        case SheetPlacement::Top:
            surface->Top(off)->Left(0);
            break;
        case SheetPlacement::Bottom:
            surface->Top(win.dipH - size - off)->Left(0);
            break;
        default:
            surface->Top(0)->Left(win.dipW - size - off);
            break;
    }
    El* head = Div(a)->FlexRow()->W(kFill)->ItemsCenter()->JustifyBetween();
    head->Child(TextEl(a, title)->Font(16)->Semibold()->Fg(th.foreground));
    head->Child(Button::New(cx, StrL("sheet-close"))
                    ->Text()
                    ->WithSize(UiSize::XSmall)
                    ->Icon(IconName::X)
                    ->OnClick(onClose)
                    ->IntoEl());
    surface->Child(head);
    if (body) {
        surface->Child(body);
    }
    El* backdrop = nullptr;
    if (overlay) {
        backdrop =
            Div(a)->Absolute()->Top(0)->Left(0)->W(win.dipW)->H(win.dipH)->Bg(
                Rgba8(0, 0, 0, 40));
        if (onClose.IsValid()) {
            backdrop->OnClick(onClose)
                ->Click(HashClickId(StrL("sheet-overlay")));
        }
    }
    El* root =
        gpui::Sheet::New(cx)->Overlay(backdrop)->Surface(surface)->IntoEl();
    // sheet.rs binds escape to Cancel in the "Sheet" context, which runs the
    // same close the backdrop and the x carry.
    CancelBindKeys(cx, root, "Sheet", StrL("sheet"), onClose);
    return root;
}

} // namespace component
} // namespace gpui
