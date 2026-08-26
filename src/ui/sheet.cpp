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
Sheet* Sheet::Footer(El* e) {
    footer = e;
    return this;
}
Sheet* Sheet::Scroll(int id, float y, Listener fn) {
    scrollId = id;
    scrollY = y;
    onScroll = fn;
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
    const Theme& th = ThemeNow(cx->app);
    Edges windowPadding = WindowPaddings(cx->win);
    float viewW = win.dipW - windowPadding.left - windowPadding.right;
    float viewH = win.dipH - windowPadding.top - windowPadding.bottom;
    bool horizontal =
        placement == SheetPlacement::Left || placement == SheetPlacement::Right;
    // The surface carries no padding of its own: the title bar, the body and
    // the footer each have theirs, which is what puts the body's scrollbar
    // against the edge and rules the footer off across the whole width.
    // theme.sheet.margin_top: a sheet that hangs from the top starts under
    // the title bar rather than over it. One rising from the bottom does not.
    const float kMarginTop = 34.f;
    float marginTop = placement == SheetPlacement::Bottom ? 0.f : kMarginTop;
    El* surface = Div(a)
                      ->Absolute()
                      ->W(horizontal ? size : viewW)
                      ->H(horizontal ? viewH - marginTop : size)
                      ->FlexCol()
                      ->Bg(th.tokens.background)
                      ->Border(1, th.border);
    // sheet.rs's "slide": 0.15 s from a hundred pixels off its own edge to
    // where it belongs. GPUI's default easing is linear, and the offset is
    // whichever way the sheet came in from.
    float delta = MotionAppear(cx, MotionId(StrL("sheet")), kSheetMotionMs);
    float off = -100.f + delta * 100.f;
    switch (placement) {
        case SheetPlacement::Left:
            surface->Top(marginTop)->Left(off);
            break;
        case SheetPlacement::Top:
            surface->Top(marginTop + off)->Left(0);
            break;
        case SheetPlacement::Bottom:
            surface->Top(viewH - size - off)->Left(0);
            break;
        default:
            surface->Top(marginTop)->Left(viewW - size - off);
            break;
    }
    El* head = Div(a)
                   ->FlexRow()
                   ->W(kFill)
                   ->Shrink0()
                   ->PadL(16)
                   ->PadR(12)
                   ->PadY(8)
                   ->ItemsCenter()
                   ->JustifyBetween();
    head->Child(TextEl(a, title)->Font(16)->Semibold()->Fg(th.foreground));
    head->Child(Button::New(cx, StrL("sheet-close"))
                    ->Ghost()
                    ->WithSize(UiSize::Small)
                    ->Icon(IconName::X)
                    ->OnClick(onClose)
                    ->IntoEl());
    surface->Child(head);
    if (body) {
        El* pane = Div(a)
                       ->FlexCol()
                       ->W(kFill)
                       ->Flex1()
                       ->MinH(0)
                       ->ClipY()
                       ->PadX(16)
                       ->Child(body);
        if (scrollId) {
            pane->ScrollY(scrollY)->ScrollId(scrollId)->OnScroll(onScroll);
        }
        surface->Child(pane);
    }
    if (footer) {
        surface->Child(Div(a)
                           ->FlexRow()
                           ->W(kFill)
                           ->Shrink0()
                           ->PadX(16)
                           ->PadY(12)
                           ->ItemsCenter()
                           ->JustifyBetween()
                           ->Child(footer));
    }
    El* backdrop = nullptr;
    if (overlay) {
        backdrop =
            Div(a)->Absolute()->Top(0)->Left(0)->W(viewW)->H(viewH)->Bg(
                Rgba8(0, 0, 0, 40));
    }
    return gpui::Sheet::New(cx)
        ->Overlay(backdrop)
        ->Surface(surface)
        ->OnClose(onClose)
        ->IntoEl()
        ->Top(windowPadding.top)
        ->Left(windowPadding.left)
        ->W(viewW)
        ->H(viewH);
}

} // namespace component
} // namespace gpui
