#include "Story.h"

struct NativeMenuStory {
    // Which trigger has its menu open; the OS draws it in Rust, we draw our
    // own popup instead.
    int open = -1;

    static El* Render(NativeMenuStory* self, Ctx* cx);
    static void OnKey(NativeMenuStory* self, Ctx* cx, const KeyEvent* ev);
};

static void ToggleNativeMenu(NativeMenuStory* self, Ctx* cx, const ClickEvent*,
                             intptr_t which) {
    self->open = self->open == (int)which ? -1 : (int)which;
    Notify(cx);
}

// trigger(): a 96px dashed-free box with muted centered text.
static El* NativeTrigger(Ctx* cx, const char* label, int which,
                         Listener toggle) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
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
        ->OnClick(ListenerArg(toggle, which));
    return box;
}

static El* DemoMenu(Ctx* cx) {
    return component::PopupMenu::New(cx, StrL("native-demo-menu"))
        ->Menu(StrL("New"))
        ->Menu(StrL("Open..."))
        ->Menu(StrL("Save"))
        ->Separator()
        ->Menu(StrL("Quit"))
        ->IntoEl();
}

El* NativeMenuStory::Render(NativeMenuStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill)->ItemsCenter();
    Listener toggle = Listen(cx, &ToggleNativeMenu);

    El* builder =
        StorySection(cx, "Builder API",
                     "Supports disabled items, checked states, and submenus.");
    StorySectionAdd(
        builder, component::Popover::New(cx)
                     ->Trigger(NativeTrigger(cx, "Right-click here", 0, toggle))
                     ->Content(DemoMenu(cx))
                     ->Open(self->open == 0)
                     ->IntoEl()
                     ->W(520));
    page->Child(builder);

    El* items =
        StorySection(cx, "Menu Items",
                     "Existing GPUI menu definitions can be reused directly.");
    StorySectionAdd(
        items, component::Popover::New(cx)
                   ->Trigger(NativeTrigger(cx, "Right-click here", 1, toggle))
                   ->Content(DemoMenu(cx))
                   ->Open(self->open == 1)
                   ->IntoEl()
                   ->W(520));
    page->Child(items);

    El* drop = StorySection(cx, "Dropdown",
                            "A native menu can open from any anchored "
                            "control.");
    StorySectionAdd(
        drop, component::Popover::New(cx)
                  ->Trigger(component::Button::New(cx, StrL("native-dropdown"))
                                ->Label(StrL("Open Menu"))
                                ->Outline()
                                ->OnClick(ListenerArg(toggle, 2))
                                ->IntoEl())
                  ->Content(DemoMenu(cx))
                  ->Open(self->open == 2)
                  ->IntoEl());
    page->Child(drop);
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
