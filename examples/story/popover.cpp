#include "Story.h"

// Which popover is open; the default-open one starts that way.
enum {
    PopDefault = 0,
    PopDefaultOpen,
    PopForm,
    PopList,
    PopRightClick,
    PopStyle,
    PopAsync,
    PopTopLeft,
    PopTopCenter,
    PopTopRight,
    PopBottomLeft,
    PopBottomCenter,
    PopBottomRight,
    PopCount
};

struct PopoverStory {
    int open = PopDefaultOpen;
    LineInput formInput = {};
    bool seeded = false;

    static El* Render(PopoverStory* self, Ctx* cx);
    static void OnKey(PopoverStory* self, Ctx* cx, const KeyEvent* ev);
};

static void TogglePop(PopoverStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t which) {
    self->open = self->open == (int)which ? -1 : (int)which;
    Notify(cx);
}
static void SubmitForm(PopoverStory* self, Ctx* cx, const ClickEvent*) {
    self->open = -1;
    Notify(cx);
}
static void FocusFormInput(PopoverStory* self, Ctx* cx, const ClickEvent*) {
    self->formInput.focused = true;
    Notify(cx);
}

// The popover surface: p_3 over the background, bordered and rounded.
static El* PopCard(Ctx* cx, float maxW) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* card = Div(a)
                   ->FlexCol()
                   ->Gap(8)
                   ->Pad(12)
                   ->Radius(th.radiusLg)
                   ->Border(1, th.border)
                   ->Bg(th.background);
    if (maxW > 0) {
        card->MaxW(maxW);
    }
    return card;
}

static El* PopText(Ctx* cx, const char* s) {
    return StoryTxt(cx, Str(s), 14, cx->theme().foreground)->Wrap();
}

static El* PopTrigger(PopoverStory*, Ctx* cx, int which, const char* id,
                      const char* label, Listener toggle) {
    return component::Button::New(cx, Str(id))
        ->Label(Str(label))
        ->Outline()
        ->OnClick(ListenerArg(toggle, which))
        ->IntoEl();
}

El* PopoverStory::Render(PopoverStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        StrCopyZ(self->formInput.buf, (int)sizeof(self->formInput.buf),
                 "Hello");
        self->formInput.len = (int)strlen(self->formInput.buf);
    }
    if (self->formInput.focused) {
        cx->win->input = &self->formInput;
    }
    Listener toggle = Listen(cx, &TogglePop);
    El* page = Div(a)->FlexCol()->Gap(8)->W(kFill);

    El* def =
        StorySection(cx, "Default", "Display lightweight contextual content.");
    El* defCard = PopCard(cx, 600)->W(400);
    defCard->Child(PopText(cx, "Hello, this is a Popover."));
    defCard->Child(component::Separator::Horizontal(cx)->IntoEl());
    defCard->Child(PopText(cx,
                           "You can put any content here, including "
                           "text, buttons, forms, and more."));
    StorySectionAdd(def, component::Popover::New(cx)
                             ->Trigger(PopTrigger(self, cx, PopDefault, "btn",
                                                  "Popover", toggle))
                             ->Content(defCard)
                             ->Open(self->open == PopDefault)
                             ->IntoEl());
    El* openCard = PopCard(cx, 600);
    openCard->Child(
        PopText(cx, "This popover is open by default when first rendered."));
    StorySectionAdd(def, component::Popover::New(cx)
                             ->Trigger(PopTrigger(self, cx, PopDefaultOpen,
                                                  "default-open-btn",
                                                  "Default Open", toggle))
                             ->Content(openCard)
                             ->Open(self->open == PopDefaultOpen)
                             ->IntoEl());
    page->Child(def);

    El* form = StorySection(cx, "Form",
                            "Keep focus and controlled open state around a "
                            "form.");
    El* formCard = PopCard(cx, 0)->W(280);
    formCard->Child(PopText(cx, "This is a form container."));
    formCard->Child(PopText(cx, "Click submit to dismiss the popover."));
    formCard->Child(
        component::Input::New(cx, StrL("pop-form-input"), &self->formInput)
            ->OnFocus(Listen(cx, &FocusFormInput))
            ->IntoEl());
    formCard->Child(component::Button::New(cx, StrL("submit"))
                        ->Label(StrL("Submit"))
                        ->Primary()
                        ->OnClick(Listen(cx, &SubmitForm))
                        ->IntoEl());
    StorySectionAdd(form, component::Popover::New(cx)
                              ->Trigger(PopTrigger(self, cx, PopForm, "pop",
                                                   "Popup Form", toggle))
                              ->Content(formCard)
                              ->Open(self->open == PopForm)
                              ->IntoEl());
    page->Child(form);

    El* list = StorySection(cx, "List",
                            "Place a scrollable selection list in the "
                            "popover.");
    StorySectionAdd(list,
                    component::Popover::New(cx)
                        ->Trigger(PopTrigger(self, cx, PopList, "pop-list",
                                             "Popup List", toggle))
                        ->Content(component::Menu::New(cx)
                                      ->Item(StrL("Jason Lee"))
                                      ->Item(StrL("Ada Lovelace"))
                                      ->Item(StrL("Alan Turing"))
                                      ->IntoEl())
                        ->Open(self->open == PopList)
                        ->IntoEl());
    page->Child(list);

    El* right = StorySection(cx, "Right click",
                             "Open from the secondary mouse button.");
    El* rightCard = PopCard(cx, 600);
    rightCard
        ->Child(PopText(cx, "Hello, this is a Popover on the Bottom Right."));
    rightCard->Child(component::Separator::Horizontal(cx)->IntoEl());
    rightCard->Child(component::Button::New(cx, StrL("info1"))
                         ->Label(StrL("Info"))
                         ->Primary()
                         ->IntoEl());
    // Ours opens on the primary button: the window routes right-clicks to the
    // whole page, not to the element under the cursor.
    StorySectionAdd(
        right, component::Popover::New(cx)
                   ->Trigger(PopTrigger(self, cx, PopRightClick, "btn-right",
                                        "Right Click Popover", toggle))
                   ->Content(rightCard)
                   ->Open(self->open == PopRightClick)
                   ->IntoEl());
    page->Child(right);

    El* style = StorySection(cx, "Custom style",
                             "Customize appearance, radius, and shadow.");
    // appearance(false) with a primary background and half the radius.
    El* styleCard = Div(a)
                        ->MaxW(600)
                        ->PadX(8)
                        ->PadY(4)
                        ->Radius(th.radius * 0.5f)
                        ->Bg(th.primary)
                        ->Child(StoryTxt(cx,
                                         StrL("A styled Popover with custom "
                                              "background and text color."),
                                         14, th.primaryFg)
                                    ->Wrap());
    StorySectionAdd(style,
                    component::Popover::New(cx)
                        ->Trigger(PopTrigger(self, cx, PopStyle, "btn-style",
                                             "Style Popover", toggle))
                        ->Content(styleCard)
                        ->Open(self->open == PopStyle)
                        ->IntoEl());
    page->Child(style);

    El* async = StorySection(cx, "Async submenu",
                             "Rebuild submenu content after asynchronous "
                             "loading.");
    StorySectionAdd(async,
                    component::Popover::New(cx)
                        ->Trigger(PopTrigger(self, cx, PopAsync, "async-menu",
                                             "Async Menu", toggle))
                        ->Content(component::Menu::New(cx)
                                      ->Item(StrL("Loading..."))
                                      ->IntoEl())
                        ->Open(self->open == PopAsync)
                        ->IntoEl());
    page->Child(async);

    El* anchor = StorySection(cx, "Anchor",
                              "Position content from each edge of the "
                              "trigger.");
    // A 360px band with a row of triggers pinned to each edge.
    El* band = Div(a)->FlexCol()->W(kFill)->H(360)->JustifyBetween();
    struct AnchorRow {
        int slots[3];
        const char* labels[3];
    };
    AnchorRow rows[2] = {
        {{PopTopLeft, PopTopCenter, PopTopRight},
         {"TopLeft", "TopCenter", "TopRight"}},
        {{PopBottomLeft, PopBottomCenter, PopBottomRight},
         {"BottomLeft", "BottomCenter", "BottomRight"}},
    };
    for (int r = 0; r < 2; r++) {
        El* row =
            Div(a)->FlexRow()->W(kFill)->H(40)->ItemsCenter()->JustifyBetween();
        for (int i = 0; i < 3; i++) {
            El* card = PopCard(cx, 600);
            card->Child(StoryTxt(cx,
                                 StoryFmt(cx, "Anchored to the trigger's %s.",
                                          rows[r].labels[i]),
                                 14, th.foreground));
            row->Child(component::Popover::New(cx)
                           ->Trigger(PopTrigger(self, cx, rows[r].slots[i],
                                                rows[r].labels[i],
                                                rows[r].labels[i], toggle))
                           ->Content(card)
                           ->Open(self->open == rows[r].slots[i])
                           ->IntoEl());
        }
        band->Child(row);
    }
    StorySectionAdd(anchor, band);
    page->Child(anchor);
    return page;
}

// Esc closes what this page has open, like an overlay dismiss.
void PopoverStory::OnKey(PopoverStory* self, Ctx* cx, const KeyEvent* ev) {
    if (ev->vk != KeyEscape) {
        return;
    }
    self->open = -1;
    Notify(cx);
}

STORY_PAGE_KEYS(StoryPopover, PopoverStory);
