#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

// crates/base/examples/showcase/components/nav_stack.rs. One page of the
// stack. A page knows its depth and holds the stack it lives in, so its own
// buttons can push over it, replace it, or pop it.
struct ScNavPage {
    int depth = 1;
    Entity<NavStackState> stack = {};

    static El* Render(ScNavPage* self, Ctx* cx);
    static void OnPush(ScNavPage* self, Ctx* cx, const ClickEvent*);
    static void OnReplace(ScNavPage* self, Ctx* cx, const ClickEvent*);
    static void OnPop(ScNavPage* self, Ctx* cx, const ClickEvent*);
    static void OnForward(ScNavPage* self, Ctx* cx, const ClickEvent*);
};

static Entity<ScNavPage> ScNavPageNew(App* app, int depth,
                                      Entity<NavStackState> stack) {
    Entity<ScNavPage> e = EntityNew<ScNavPage>(app);
    ScNavPage* page = e.Get(app);
    if (page) {
        page->depth = depth;
        page->stack = stack;
    }
    return e;
}

// A pushed page sits one deeper, a replacement at the same depth.
void ScNavPage::OnPush(ScNavPage* self, Ctx* cx, const ClickEvent*) {
    NavStackState* s = self->stack.Get(cx);
    if (!s) {
        return;
    }
    Entity<ScNavPage> page =
        ScNavPageNew(cx->app, self->depth + 1, self->stack);
    NavStackPush(s, cx, page.id, NavMotion::Animated);
}

void ScNavPage::OnReplace(ScNavPage* self, Ctx* cx, const ClickEvent*) {
    NavStackState* s = self->stack.Get(cx);
    if (!s) {
        return;
    }
    Entity<ScNavPage> page = ScNavPageNew(cx->app, self->depth, self->stack);
    NavStackReplace(s, cx, page.id, NavMotion::Animated);
}

void ScNavPage::OnPop(ScNavPage* self, Ctx* cx, const ClickEvent*) {
    NavStackState* s = self->stack.Get(cx);
    if (s) {
        NavStackPop(s, cx, NavMotion::Animated);
    }
}

void ScNavPage::OnForward(ScNavPage* self, Ctx* cx, const ClickEvent*) {
    NavStackState* s = self->stack.Get(cx);
    if (s) {
        NavStackForward(s, cx, NavMotion::Animated);
    }
}

static El* ScNavButton(Ctx* cx, Str id, Str label, Listener onClick) {
    return ScButton(cx, id)
        ->H(28)
        ->PadX(8)
        ->FlexRow()
        ->ItemsCenter()
        ->Border(1, ScInk())
        ->Bg(ScWhite())
        ->OnClick(onClick)
        ->Child(ScTxt(cx, label, 12, ScInk()));
}

El* ScNavPage::Render(ScNavPage* self, Ctx* cx) {
    Arena* a = cx->a;
    int depth = self->depth;
    // The trail is the stack's History: the pages behind this one, then the
    // pages popped off it, which Forward brings back one at a time.
    int behind = depth;
    int ahead = 0;
    if (NavStackState* s = self->stack.Get(cx)) {
        behind = s->Depth();
        ahead = s->ForwardCount();
    }

    El* root = Div(a)->SizeFull()->FlexCol()->Gap(12)->Pad(12)->Bg(
        depth % 2 == 1 ? ScWhite() : ScHover());
    root->Child(Div(a)->Child(
        ScTxt(cx, DupFmt(cx, "Page %d", depth), 12, ScInk())->Semibold()));

    El* trail = Div(a)->FlexRow()->Gap(4);
    for (int page = 1; page <= behind + ahead; page++) {
        Rgba fg = ScMutedC();
        bool semibold = false;
        if (page == depth) {
            fg = ScInk();
            semibold = true;
        }
        if (page > behind) {
            fg = ScBorder();
        }
        El* text = ScTxt(cx, DupFmt(cx, "%d", page), 12, fg);
        if (semibold) {
            text->Semibold();
        }
        trail->Child(Div(a)->PadX(4)->Child(text));
    }
    root->Child(trail);

    El* row = Div(a)->FlexRow()->Gap(8);
    row->Child(ScNavButton(cx, DupFmt(cx, "nav-push-%d", depth), StrL("Push"),
                           Listen(cx, &ScNavPage::OnPush)));
    row->Child(ScNavButton(cx, DupFmt(cx, "nav-replace-%d", depth),
                           StrL("Replace"), Listen(cx, &ScNavPage::OnReplace)));
    if (depth > 1) {
        row->Child(ScNavButton(cx, DupFmt(cx, "nav-pop-%d", depth), StrL("Pop"),
                               Listen(cx, &ScNavPage::OnPop)));
    }
    if (ahead > 0) {
        row->Child(ScNavButton(cx, DupFmt(cx, "nav-forward-%d", depth),
                               StrL("Forward"),
                               Listen(cx, &ScNavPage::OnForward)));
    }
    root->Child(row);
    return root;
}

// A pushed page slides in from the right and slides back out when popped; the
// page underneath drifts a little to show depth. A replacement slides in over
// the page it replaces.
static El* ScNavSlide(void*, Ctx*, const NavPage& page) {
    float offset = 0.f;
    if (page.HasOperation()) {
        NavOperation op = page.Operation();
        switch (page.Phase()) {
            case PresencePhase::Entering:
                if (op == NavOperation::Push || op == NavOperation::Replace) {
                    offset = 1.f - page.Progress();
                } else if (op == NavOperation::Pop) {
                    offset = -0.3f * (1.f - page.Progress());
                }
                break;
            case PresencePhase::Exiting:
                if (op == NavOperation::Pop) {
                    offset = page.Progress();
                } else if (op == NavOperation::Push) {
                    offset = -0.3f * page.Progress();
                }
                break;
            default:
                break;
        }
    }
    return page.el->LeftRel(offset);
}

El* ShowcaseNavStack(ShowcaseApp* app, Ctx* cx) {
    if (!app->navStack.IsValid()) {
        app->navStack = NavStackStateNew(cx->app);
        NavStackState* s = app->navStack.Get(cx);
        Entity<ScNavPage> root = ScNavPageNew(cx->app, 1, app->navStack);
        // Into an empty stack a push is immediate, like Qt's initialItem.
        NavStackPush(s, cx, root.id, NavMotion::Immediate);
    }
    // w_72 / h_40 in the Rust page.
    return NavStack::New(cx, app->navStack)
        ->Transition(motion::Transition::New(220))
        ->Item(&ScNavSlide)
        ->IntoEl()
        ->W(288)
        ->H(160)
        ->ClipX()
        ->ClipY()
        ->Border(1, ScBorder());
}

SHOWCASE_PAGE(CompNavStack, ShowcaseNavStack);
