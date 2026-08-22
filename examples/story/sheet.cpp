#include "Story.h"

enum {
    SheetLeft = 0,
    SheetTop,
    SheetRight,
    SheetBottom,
    SheetScrollable,
    SheetNone = -1
};

enum {
    SheetOptOverlay = ToolbarOptMultiple,
    SheetOptOverlayClosable = ToolbarOptIcon
};

struct SheetStory {
    int open = SheetNone;
    bool overlay = true;
    bool overlayClosable = true;
    InputState focusInput;
    StoryToolbarState toolbar;
    bool seeded = false;

    static El* Render(SheetStory* self, Ctx* cx);
};

static void SheetToolbarAct(SheetStory* self, Ctx* cx, const ClickEvent*,
                            intptr_t act) {
    if (act == SheetOptOverlay) {
        self->overlay = !self->overlay;
    } else if (act == SheetOptOverlayClosable) {
        self->overlayClosable = !self->overlayClosable;
    } else {
        StoryToolbarApply(&self->toolbar, nullptr, (int)act);
    }
    Notify(cx);
}

static void OpenSheet(SheetStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t which) {
    self->open = (int)which;
    Notify(cx);
}
static void CloseSheet(SheetStory* self, Ctx* cx, const ClickEvent*) {
    self->open = SheetNone;
    Notify(cx);
}
static void FocusSheetInput(SheetStory* self, Ctx* cx, const ClickEvent*) {
    self->focusInput.focused = true;
    Notify(cx);
}

El* SheetStory::Render(SheetStory* self, Ctx* cx) {
    WinSize size = WindowSize(cx->win);
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        InputSetPlaceholder(&self->focusInput,
                            StrL("For test focus back on dialog close."));
    }
    if (self->focusInput.focused) {
        cx->win->input = &self->focusInput;
    }
    Listener open = Listen(cx, &OpenSheet);
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    StoryToolbarOpt opts[2] = {
        {"Overlay", self->overlay, SheetOptOverlay},
        {"Close on overlay click", self->overlayClosable,
         SheetOptOverlayClosable},
    };
    // story_toolbar_group(): this page has no size button.
    page->Child(StoryToolbarOptions(cx, self, opts, 2,
                                    Listen(cx, &SheetToolbarAct), false));

    El* place = StorySection(cx, "Placement",
                             "Open a sheet from any edge of the window.");
    const char* ids[] = {"show-sheet-left", "show-sheet-top",
                         "show-sheet-right", "show-sheet-bottom"};
    const char* labels[] = {"Left Sheet...", "Top Sheet...", "Right Sheet...",
                            "Bottom Sheet..."};
    for (int i = 0; i < 4; i++) {
        StorySectionAdd(place, component::Button::New(cx, Str(ids[i]))
                                   ->Label(Str(labels[i]))
                                   ->Outline()
                                   ->OnClick(ListenerArg(open, i))
                                   ->IntoEl());
    }
    page->Child(place);

    El* scroll = StorySection(cx, "Scrollable Sheet", nullptr);
    StorySectionBody(scroll)->W(480);
    StorySectionAdd(scroll,
                    component::Button::New(cx, StrL("show-scrollable-sheet"))
                        ->Label(StrL("Scrollable Sheet..."))
                        ->Outline()
                        ->OnClick(ListenerArg(open, SheetScrollable))
                        ->IntoEl());
    page->Child(scroll);

    // w_128 with the section gap: the input fills the row and the button
    // wraps under it.
    El* focus = StorySection(cx, "Focus back test", nullptr);
    StorySectionBody(focus)->W(512);
    StorySectionAdd(focus, component::Input::New(cx, StrL("sheet-focus-input"),
                                                 &self->focusInput)
                               ->OnFocus(Listen(cx, &FocusSheetInput))
                               ->IntoEl()
                               ->W(512));
    StorySectionAdd(focus,
                    component::Button::New(cx, StrL("test-action"))
                        ->Label(StrL("Test Action"))
                        ->Outline()
                        ->Tooltip(StrL("This button for test dispatch action, "
                                       "to make sure when Dialog close, this "
                                       "still can handle the action."))
                        ->IntoEl());
    page->Child(focus);

    if (self->open != SheetNone) {
        component::SheetPlacement placement =
            self->open == SheetLeft     ? component::SheetPlacement::Left
            : self->open == SheetTop    ? component::SheetPlacement::Top
            : self->open == SheetBottom ? component::SheetPlacement::Bottom
                                        : component::SheetPlacement::Right;
        El* body = Div(a)->FlexCol()->Gap(8)->W(kFill);
        if (self->open == SheetScrollable) {
            // "This is a scrollable sheet." repeated past the sheet height.
            for (int i = 0; i < 20; i++) {
                body->Child(StoryTxt(cx, StrL("This is a scrollable sheet."),
                                     14, th.foreground));
            }
        } else {
            body->Child(StoryTxt(cx,
                                 StrL("This is a sheet, you can put any "
                                      "content here."),
                                 14, th.foreground)
                            ->Wrap());
        }
        page->Child(component::Sheet::New(cx)
                        ->Open(true)
                        ->Title(self->open == SheetScrollable
                                    ? StrL("Scrollable Sheet")
                                    : StrL("Sheet Title"))
                        ->Placement(placement)
                        ->Overlay(self->overlay)
                        ->Body(body)
                        ->OnClose(Listen(cx, &CloseSheet))
                        ->IntoEl(size));
    }
    return page;
}

STORY_PAGE(StorySheet, SheetStory);
