#include "Story.h"

struct NativeMenuStory {
    // Which trigger has a drawn menu open. Only used where the platform has
    // no menu of its own — elsewhere the OS draws it, over the window and
    // outside it, and this page holds nothing.
    int open = -1;
    // What the last chosen row reported, which is Rust's dispatched action.
    intptr_t picked = 0;

    static El* Render(NativeMenuStory* self, Ctx* cx);
    static void OnKey(NativeMenuStory* self, Ctx* cx, const KeyEvent* ev);
};

// The rows every trigger on this page opens: a checked row, a greyed one and
// a submenu, which is what the Rust story shows off.
static component::NativeMenu* DemoMenu(Ctx* cx, NativeMenuStory* self) {
    component::NativeMenu* sub =
        component::NativeMenu::New(cx)
            ->Menu(StrL("Copy"), 10)
            ->Menu(StrL("Cut"), 11)
            ->MenuWithDisabled(StrL("Paste"), true, 12);
    return component::NativeMenu::New(cx)
        ->Menu(StrL("New"), 1)
        ->Menu(StrL("Open..."), 2)
        ->MenuWithCheck(StrL("Word Wrap"), self->picked == 3, 3)
        ->Separator()
        ->Submenu(StrL("Edit"), sub)
        ->Separator()
        ->MenuWithDisabled(StrL("Save"), true, 4)
        ->Menu(StrL("Quit"), 5);
}

static void OnMenuSelect(NativeMenuStory* self, Ctx* cx, const ClickEvent*,
                         intptr_t id) {
    self->picked = id;
    Notify(cx);
}

// A press that opens the menu where the pointer is. The OS menu is not
// clipped to the window, so it opens at the press; where the platform has
// none, the page draws the same rows under the trigger instead.
static void OnTriggerDown(NativeMenuStory* self, Ctx* cx,
                          const MouseDownEvent* ev, intptr_t which) {
    component::NativeMenu* menu = DemoMenu(cx, self);
    menu->OnSelect(Listen(cx, &OnMenuSelect));
    if (menu->Show(ev->x, ev->y)) {
        self->open = -1;
    } else {
        self->open = self->open == (int)which ? -1 : (int)which;
    }
    Notify(cx);
}

static void OnTriggerClick(NativeMenuStory* self, Ctx* cx, const ClickEvent* ev,
                           intptr_t which) {
    MouseDownEvent down = {};
    down.x = ev->x;
    down.y = ev->y;
    OnTriggerDown(self, cx, &down, which);
}

// The same rows drawn, for a platform with no menu of its own — Rust's
// FallbackMenuOverlay, which Root holds and anchors at the press.
static El* FallbackMenu(Ctx* cx, NativeMenuStory* self, int which) {
    return DemoMenu(cx, self)
        ->IntoPopupMenu(StoryFmt(cx, "native-fallback-%d", which))
        ->IntoEl();
}

// trigger(): a 96px box with muted centered text, with the drawn menu under
// it on the platforms that need one.
static El* NativeTrigger(Ctx* cx, NativeMenuStory* self, const char* label,
                         int which) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* wrap = Div(a)->W(kFill);
    El* box = Div(a)
                  ->FlexRow()
                  ->W(kFill)
                  ->H(96)
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Radius(th.radiusLg)
                  ->Border(1, th.border)
                  ->Child(StoryTxt(cx, Str(label), 16, th.mutedFg));
    box->Click(HashClickId(StoryFmt(cx, "native-trigger-%d", which)))
        ->OnMouseDown(ListenerArg(Listen(cx, &OnTriggerDown), which));
    wrap->Child(box);
    if (self->open == which) {
        wrap->Child(
            FallbackMenu(cx, self, which)->Absolute()->Top(100)->Left(0));
    }
    return wrap;
}

El* NativeMenuStory::Render(NativeMenuStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill)->ItemsCenter();

    El* builder =
        StorySection(cx, "Builder API",
                     "Supports disabled items, checked states, and submenus.");
    StorySectionAdd(builder, NativeTrigger(cx, self, "Click here", 0)->W(520));
    page->Child(builder);

    El* items = StorySection(
        cx, "Menu Items",
        "Each row carries what it reports, dispatched when one is chosen.");
    StorySectionAdd(items, NativeTrigger(cx, self, "Click here", 1)->W(520));
    page->Child(items);

    El* drop = StorySection(cx, "Dropdown",
                            "A native menu can open from any anchored "
                            "control.");
    El* wrap = Div(a)->FlexCol()->ItemsCenter();
    wrap->Child(component::Button::New(cx, StrL("native-dropdown"))
                    ->Label(StrL("Open Menu"))
                    ->Outline()
                    ->OnClick(ListenerArg(Listen(cx, &OnTriggerClick), 2))
                    ->IntoEl());
    if (self->open == 2) {
        wrap->Child(FallbackMenu(cx, self, 2)->Absolute()->Top(36)->Left(0));
    }
    StorySectionAdd(drop, wrap);
    page->Child(drop);

    // What the last chosen row reported, so the page shows the dispatch
    // happened even where the menu itself was the OS's.
    Str chosen = self->picked
                     ? StoryFmt(cx, "Selected id: %d", (int)self->picked)
                     : StrL("Nothing selected yet");
    page->Child(StoryTxt(cx, chosen, 14, th.mutedFg));
    return page;
}

// Esc closes what this page has open, like an overlay dismiss.
void NativeMenuStory::OnKey(NativeMenuStory* self, Ctx* cx,
                            const KeyEvent* ev) {
    if (ev->vk != KeyEscape) {
        return;
    }
    self->open = -1;
    Notify(cx);
}

STORY_PAGE_KEYS(StoryNativeMenu, NativeMenuStory);
