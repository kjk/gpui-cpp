#include "Story.h"

// One entry per section, in the order the Rust story renders them.
enum {
    DlgDefault = 0,
    DlgCustomButtons,
    DlgScrollable,
    DlgTable,
    DlgNoTitle,
    DlgPadding,
    DlgStyle,
    DlgContent,
    DlgTextView,
    DlgCount
};

enum {
    DlgOptOverlay = ToolbarOptMultiple,
    DlgOptOverlayClosable = ToolbarOptIcon,
    DlgOptCloseButton = ToolbarOptDisabled,
    DlgOptKeyboard = ToolbarOptBordered
};

struct DialogStory {
    int open = -1;
    bool overlay = true;
    bool overlayClosable = true;
    bool closeButton = true;
    bool keyboard = true;
    InputState focusInput;
    StoryToolbarState toolbar;

    static El* Render(DialogStory* self, Ctx* cx);
    static void OnKey(DialogStory* self, Ctx* cx, const KeyEvent* ev);
};

static void DlgToolbarAct(DialogStory* self, Ctx* cx, const ClickEvent*,
                          intptr_t act) {
    switch (act) {
        case DlgOptOverlay:
            self->overlay = !self->overlay;
            break;
        case DlgOptOverlayClosable:
            self->overlayClosable = !self->overlayClosable;
            break;
        case DlgOptCloseButton:
            self->closeButton = !self->closeButton;
            break;
        case DlgOptKeyboard:
            self->keyboard = !self->keyboard;
            break;
        default:
            StoryToolbarApply(&self->toolbar, nullptr, (int)act);
            break;
    }
    Notify(cx);
}

static void OpenDialog(DialogStory* self, Ctx* cx, const ClickEvent*,
                       intptr_t which) {
    self->open = (int)which;
    Notify(cx);
}
static void CloseDlg(DialogStory* self, Ctx* cx, const ClickEvent*) {
    self->open = -1;
    Notify(cx);
}
static void FocusInput(DialogStory* self, Ctx* cx, const ClickEvent*) {
    self->focusInput.focused = true;
    Notify(cx);
}

static El* DlgTrigger(Ctx* cx, int which, const char* id, const char* label,
                      Listener open) {
    return component::Button::New(cx, Str(id))
        ->Label(Str(label))
        ->Outline()
        ->OnClick(ListenerArg(open, which))
        ->IntoEl();
}

El* DialogStory::Render(DialogStory* self, Ctx* cx) {
    WinSize size = WindowSize(cx->win);
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (self->focusInput.focused) {
        cx->win->input = &self->focusInput;
    }
    Listener open = Listen(cx, &OpenDialog);
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    StoryToolbarOpt opts[4] = {
        {"Overlay", self->overlay, DlgOptOverlay},
        {"Close on overlay click", self->overlayClosable,
         DlgOptOverlayClosable},
        {"Close button", self->closeButton, DlgOptCloseButton},
        {"Keyboard", self->keyboard, DlgOptKeyboard},
    };
    // story_toolbar_group(): this page has no size button.
    page->Child(StoryToolbarOptions(cx, self, opts, 4,
                                    Listen(cx, &DlgToolbarAct), false));

    struct SectionSpec {
        int which;
        const char* title;
        const char* description;
        const char* id;
        const char* label;
    };
    static const SectionSpec kSections[] = {
        {DlgDefault, "Default", "Compose form controls and footer actions.",
         "show-dialog", "Open Dialog"},
        {DlgCustomButtons, "Custom actions",
         "Replace the default footer actions.", "confirm-dialog1",
         "Custom Buttons"},
        {DlgScrollable, "Scrollable",
         "Keep long content inside a fixed dialog size.", "scrollable-dialog",
         "Scrollable Dialog"},
        {DlgTable, "Data table", "Embed a full interactive component.",
         "table-dialog", "Table Dialog"},
        {DlgNoTitle, "Without title", "Render content without a heading.",
         "dialog-no-title", "Dialog without Title"},
        {DlgPadding, "Padding", "Control spacing around dialog content.",
         "custom-dialog-paddings", "Custom Paddings"},
        {DlgStyle, "Custom style", "Customize color, radius, and foreground.",
         "custom-dialog-style", "Custom Dialog Style"},
        {DlgContent, "Custom content",
         "Compose header, body, and footer explicitly.",
         "custom-width-dialog-btn", "Custom Width (400px)"},
        {DlgTextView, "Selectable text", "Embed selectable rich text.",
         "textview-dialog-btn", "TextView Dialog"},
    };
    for (size_t i = 0; i < sizeof(kSections) / sizeof(kSections[0]); i++) {
        const SectionSpec& s = kSections[i];
        El* sec = StorySection(cx, s.title, s.description);
        StorySectionAdd(sec, DlgTrigger(cx, s.which, s.id, s.label, open));
        page->Child(sec);
    }

    // The focus-return check is a bare card, not a section.
    El* focusRow = Div(a)->FlexRow()->W(kFill)->JustifyCenter();
    El* focusCard = Div(a)
                        ->FlexCol()
                        ->W(384)
                        ->Gap(12)
                        ->Pad(12)
                        ->Radius(th.radiusLg)
                        ->Bg(RgbaOpacity(th.muted, 0.45f))
                        ->Border(1, th.border);
    El* focusHead = Div(a)->FlexCol()->Gap(4);
    focusHead->Child(StoryTxt(cx, StrL("Focus return check"), 16, th.foreground)
                         ->Medium());
    focusHead
        ->Child(StoryTxt(cx, StrL("Type here, then open and close any dialog."),
                         12, th.mutedFg));
    focusCard->Child(focusHead);
    El* focusRowInner = Div(a)->FlexRow()->W(kFill)->Gap(8)->ItemsCenter();
    focusRowInner->Child(Div(a)->Grow()->Child(
        component::Input::New(cx, StrL("focus-input"), &self->focusInput)
            ->OnFocus(Listen(cx, &FocusInput))
            ->IntoEl()));
    focusRowInner->Child(component::Button::New(cx, StrL("test-action"))
                             ->Label(StrL("Run Action"))
                             ->Outline()
                             ->Tooltip(StrL("Verify actions still dispatch "
                                            "after a dialog closes."))
                             ->IntoEl());
    focusCard->Child(focusRowInner);
    focusRow->Child(focusCard);
    page->Child(focusRow);

    if (self->open >= 0) {
        component::Dialog* d = component::Dialog::New(cx)
                                   ->Open(true)
                                   ->OnClose(Listen(cx, &CloseDlg))
                                   ->OnOk(Listen(cx, &CloseDlg));
        switch (self->open) {
            case DlgNoTitle:
                d->Description(
                    StrL("This is a dialog without title, you can "
                         "use it when the title is not "
                         "necessary."));
                break;
            case DlgCustomButtons:
                d->Description(StrL("Are you sure you want to continue?"));
                break;
            case DlgScrollable:
                d->Title(StrL("Dialog with scrollbar"));
                d
                    ->Description(StrL("The body scrolls when the content is "
                                       "taller than the dialog."));
                break;
            case DlgTable:
                d->Title(StrL("Dialog with Table"));
                d->Description(
                    StrL("A full data table lives inside the dialog."));
                break;
            case DlgPadding:
                d->Title(StrL("Custom Dialog Title"));
                d->Description(
                    StrL("This is a custom dialog content, we can "
                         "use paddings to control the layout and "
                         "spacing within the dialog."));
                break;
            case DlgStyle:
                d->Title(StrL("Custom Dialog Title"));
                d->Description(StrL("This is a custom dialog content."));
                break;
            case DlgContent:
                d->Title(StrL("Custom Width"));
                d->Description(
                    StrL("This dialog has a custom width of 400px."));
                break;
            case DlgTextView:
                d->Title(StrL("TextView Dialog"));
                d
                    ->Description(StrL("This is a dialog with a selectable "
                                       "TextView in it. This text should be "
                                       "selectable."));
                break;
            default:
                d->Title(StrL("Open Dialog"));
                d->Description(
                    StrL("Compose form controls and footer actions."));
                break;
        }
        page->Child(d->IntoEl(size));
    }
    return page;
}

// Esc closes what this page has open, like an overlay dismiss.
void DialogStory::OnKey(DialogStory* self, Ctx* cx, const KeyEvent* ev) {
    if (ev->vk != KeyEscape || !self->keyboard) {
        return;
    }
    self->open = -1;
    Notify(cx);
}

STORY_PAGE_KEYS(StoryDialog, DialogStory);
