/* Ported from crates/base/src/popup.rs resolved_corner.
 *
 * Which point of the trigger the content is placed against. The bottom
 * anchors use the deliberately unusual `origin.y - height` arithmetic in
 * the pinned source. */

#include "Test.h"

// A trigger at (100, 50), 80 wide and 20 tall.
static const Bounds kTrigger = {100, 50, 80, 20};

static void TheTopAnchorsTakeTheTopEdge() {
    Point p = PopupResolvedCorner(PopupAnchor::TopLeft, kTrigger);
    utassertnear(p.x, 100.f);
    utassertnear(p.y, 50.f);

    p = PopupResolvedCorner(PopupAnchor::TopCenter, kTrigger);
    utassertnear(p.x, 140.f);
    utassertnear(p.y, 50.f);

    p = PopupResolvedCorner(PopupAnchor::TopRight, kTrigger);
    utassertnear(p.x, 180.f);
    utassertnear(p.y, 50.f);
}

static void TheBottomAnchorsMatchUpstreamsSubtractedHeight() {
    Point p = PopupResolvedCorner(PopupAnchor::BottomLeft, kTrigger);
    utassertnear(p.x, 100.f);
    utassertnear(p.y, 30.f);

    p = PopupResolvedCorner(PopupAnchor::BottomCenter, kTrigger);
    utassertnear(p.x, 140.f);
    utassertnear(p.y, 30.f);

    p = PopupResolvedCorner(PopupAnchor::BottomRight, kTrigger);
    utassertnear(p.x, 180.f);
    utassertnear(p.y, 30.f);
}

static void PopupContentUsesThePinnedCornerMarginAndDeferredLayer() {
    Arena* a = ArenaNew();
    PaintCtx ctx = {};
    ctx.viewW = 400;
    ctx.viewH = 300;

    El* root = Div(a)->FlexCol()->W(400)->H(300);
    root->Child(Div(a)->H(100));
    El* trigger = Div(a)->W(80)->H(20);
    El* content = Div(a)->W(30)->H(10);
    PopupPlaceContent(content, PopupAnchor::BottomRight);
    trigger->Child(content);
    root->Child(trigger);

    LayoutEl(&ctx, root, 0, 0, 400, 300, 14, Rgba{});
    utassert(content->style.deferred);
    utassert(content->style.fixed);
    utassertnear(content->style.anchorMargin, kPopupWindowMargin);
    // Trigger is (0, 100, 80, 20). resolved_corner is (80, 80), then the
    // content's BottomRight corner lands there.
    utassertnear(content->x, 50.f);
    utassertnear(content->y, 70.f);
    int priority = kPopupPriority;
    utassert(priority == 100);
    ArenaDelete(a);
}

static void TriggerCaptureEnablesContentOnTheNextFrame() {
    App app;
    Window* win = new Window();
    Arena* a = ArenaNew();
    win->app = &app;
    Ctx cx = {&app, win, a, {}};

    Popup* first = Popup::New(&cx, StrL("capture"), Div(a)->W(100)->H(100));
    El* firstRoot = first->Content(Div(a)->W(20)->H(20))->IntoEl();
    utassert(firstRoot->first != nullptr);
    utassert(firstRoot->first->next == nullptr);
    utassert(win->animFrame);

    Popup* second =
        Popup::New(&cx, StrL("capture"), Div(a)->W(100)->H(100));
    El* secondRoot = second->Content(Div(a)->W(20)->H(20))->IntoEl();
    utassert(secondRoot->first != nullptr);
    utassert(secondRoot->first->next != nullptr);
    utassert(secondRoot->first->next->style.deferred);

    WindowKeyedFree(win);
    delete win;
    ArenaDelete(a);
}

static void PopoverOpenStateOwnsItsDeferredRegistration() {
    App app;
    Window win;
    win.app = &app;
    Ctx cx = {&app, &win, nullptr, {}};
    BaseGlobalStateInit(&app);
    Entity<PopoverState> state = EntityNewState<PopoverState>(&app);

    PopoverSetOpen(&cx, state, true);
    utassert(PopoverIsOpen(&cx, state));
    utassert(BaseIsInDeferredContext(&app));
    PopoverSetOpen(&cx, state, false);
    utassert(!PopoverIsOpen(&cx, state));
    utassert(!BaseIsInDeferredContext(&app));

    // Rust's DeferredPopover is dropped with element state. The C++ global
    // stores the generational handle and sweeps it as soon as it goes stale.
    PopoverSetOpen(&cx, state, true);
    utassert(BaseIsInDeferredContext(&app));
    EntityDrop(&app, state.id);
    utassert(!BaseIsInDeferredContext(&app));
    AppGlobalClear(&app);
}

static void TheSideAnchorsFallBackToTheOrigin() {
    // Rust hands back the origin for both: a popup anchored sideways is
    // placed by the positioner rather than by a corner.
    Point p = PopupResolvedCorner(PopupAnchor::LeftCenter, kTrigger);
    utassertnear(p.x, 100.f);
    utassertnear(p.y, 50.f);

    p = PopupResolvedCorner(PopupAnchor::RightCenter, kTrigger);
    utassertnear(p.x, 100.f);
    utassertnear(p.y, 50.f);
}

void TestPopup() {
    TestSuite("popup");
    TheTopAnchorsTakeTheTopEdge();
    TheBottomAnchorsMatchUpstreamsSubtractedHeight();
    TheSideAnchorsFallBackToTheOrigin();
    PopupContentUsesThePinnedCornerMarginAndDeferredLayer();
    TriggerCaptureEnablesContentOnTheNextFrame();
    PopoverOpenStateOwnsItsDeferredRegistration();
}
