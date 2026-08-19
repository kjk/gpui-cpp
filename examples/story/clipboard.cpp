#include "Story.h"

struct ClipboardStory {
    // The field whose value the second button copies.
    InputState url;
    bool masked = false;
    bool seeded = false;
    // Rust's on_copied pushes a window notification; there is no window-level
    // notification list here, so the page shows what it was handed instead.
    Str copied = {};

    ~ClipboardStory() { StrFree(copied); }
    static El* Render(ClipboardStory* self, Ctx* cx);
};

static void OnCopied(ClipboardStory* self, Ctx* cx,
                     const component::ClipboardEvent* ev) {
    StrFree(self->copied);
    self->copied = StrDup(ev->value);
    Notify(cx);
}

static void FocusUrl(ClipboardStory* self, Ctx* cx, const ClickEvent*) {
    cx->win->input = &self->url;
    Notify(cx);
}

El* ClipboardStory::Render(ClipboardStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        InputSetValue(&self->url, StrL("https://github.com"));
    }
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
    Listener copied = Listen(cx, &OnCopied);

    El* def = StorySection(cx, "Default",
                           "Copies a value supplied by the application.");
    El* defRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    defRow->Child(component::Label::New(cx, StrL("A clipboard button"))
                      ->IntoEl());
    // value_fn: the value is the page's own state, read at the frame the
    // click lands in.
    defRow->Child(
        component::Clipboard::New(cx, StrL("clipboard1"))
            ->Value(StoryFmt(cx, "masked :%s", self->masked ? "true" : "false"))
            ->OnCopied(copied)
            ->IntoEl());
    StorySectionAdd(def, defRow);
    page->Child(def);

    El* input =
        StorySection(cx, "With Input", "Copies the field's current value.");
    StorySectionAdd(
        input, component::Input::New(cx, StrL("url"), &self->url)
                   ->OnFocus(Listen(cx, &FocusUrl))
                   ->Suffix(component::Clipboard::New(cx, StrL("clipboard2"))
                                ->Value(InputValue(&self->url))
                                ->OnCopied(copied)
                                ->IntoEl())
                   ->IntoEl());
    page->Child(input);

    if (self->copied.s) {
        page->Child(StoryTxt(cx,
                             StoryFmt(cx, "Copied value: %.*s",
                                      self->copied.len, self->copied.s),
                             13, th.mutedFg));
    }
    return page;
}

STORY_PAGE(StoryClipboard, ClipboardStory);
