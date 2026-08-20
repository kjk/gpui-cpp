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
    DlgCount,
    DlgOther
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
    InputState basicInput;
    Entity<component::SearchableListState> basicSelect = {};
    Entity<TableState> table = {};
    LocalDate basicDate = {};
    bool basicDateOpen = false;
    float dialogScrollY = 0;
    bool seeded = false;
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
    self->basicInput.focused = false;
    self->basicDateOpen = false;
    if (component::SearchableListState* s = self->basicSelect.Get(cx)) {
        s->open = false;
    }
    Notify(cx);
}
static void FocusInput(DialogStory* self, Ctx* cx, const ClickEvent*) {
    self->focusInput.focused = true;
    self->basicInput.focused = false;
    Notify(cx);
}

static void FocusBasicInput(DialogStory* self, Ctx* cx, const ClickEvent*) {
    self->focusInput.focused = false;
    self->basicInput.focused = true;
    Notify(cx);
}

static void ToggleBasicSelect(DialogStory* self, Ctx* cx, const ClickEvent*) {
    component::SelectToggleOpen(self->basicSelect.Get(cx), cx);
}

static void ToggleBasicDate(DialogStory* self, Ctx* cx, const ClickEvent*) {
    self->basicDateOpen = !self->basicDateOpen;
    Notify(cx);
}

static void PickBasicDate(DialogStory* self, Ctx* cx, const ClickEvent*,
                          intptr_t day) {
    self->basicDate.day = (int)day;
    self->basicDateOpen = false;
    Notify(cx);
}

static void OnDialogScroll(DialogStory* self, Ctx* cx, const ScrollEvent* ev) {
    self->dialogScrollY = ev->offsetY;
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

static component::SearchableItem gDialogOptions[3];

static El* DialogTitleText(Ctx* cx, Str text, Rgba color) {
    return StoryTxt(cx, text, 16, color)->Semibold()->LineHeight(1.f);
}

static El* DialogDescriptionText(Ctx* cx, Str text) {
    return StoryTxt(cx, text, 14, cx->theme().mutedFg)->Wrap()->W(kFill);
}

static El* DialogHeader(Ctx* cx, Str title, Str description) {
    El* header = Div(cx->a)->FlexCol()->W(kFill)->Gap(8);
    if (title.len > 0) {
        header->Child(DialogTitleText(cx, title, cx->theme().foreground));
    }
    if (description.len > 0) {
        header->Child(DialogDescriptionText(cx, description));
    }
    return header;
}

static El* DialogButton(Ctx* cx, Str id, Str label, Listener onClick,
                        bool primary) {
    component::Button* button =
        component::Button::New(cx, id)->Label(label)->OnClick(onClick);
    if (primary) {
        button->Primary();
    } else {
        button->Outline();
    }
    return button->IntoEl();
}

static El* DialogTableCell(Ctx* cx, void*, int row, int col) {
    const Theme& th = cx->theme();
    switch (col) {
        case 0:
            return StoryTxt(cx, StoryFmt(cx, "%d", row), 16, th.foreground)
                ->LineHeight(1.f);
        case 1:
            return StoryTxt(cx, StoryFmt(cx, "User %d", row), 16, th.foreground)
                ->LineHeight(1.f);
        case 2:
            return StoryTxt(cx, StoryFmt(cx, "user-%d@mail.com", row), 16,
                            th.foreground)
                ->LineHeight(1.f);
        case 3:
            return StoryTxt(cx, StrL("User"), 16, th.foreground)
                ->LineHeight(1.f);
        default:
            return StoryTxt(cx, StrL("Active"), 16, th.foreground)
                ->LineHeight(1.f);
    }
}

El* DialogStory::Render(DialogStory* self, Ctx* cx) {
    WinSize size = WindowSize(cx->win);
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        InputSetPlaceholder(&self->basicInput, StrL("Your Name"));
        InputSetPlaceholder(&self->focusInput,
                            StrL("Type before opening a dialog"));
        self->basicSelect =
            EntityNewState<component::SearchableListState>(cx->app);
        self->table = EntityNewState<TableState>(cx->app);
        self->basicDate = DateToday();
        self->basicDate.day = 0;
        static const char* kOptions[] = {"Option 1", "Option 2", "Option 3"};
        for (int i = 0; i < 3; i++) {
            gDialogOptions[i].title = Str(kOptions[i]);
            gDialogOptions[i].value = Str(kOptions[i]);
        }
    }
    if (self->basicInput.focused) {
        cx->win->input = &self->basicInput;
    } else if (self->focusInput.focused) {
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
    focusHead->Child(
        StoryTxt(cx, StrL("Focus return check"), 16, th.foreground)->Medium());
    focusHead->Child(
        StoryTxt(cx, StrL("Type here, then open and close any dialog."), 12,
                 th.mutedFg));
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
        Listener close = Listen(cx, &CloseDlg);
        component::Dialog* d = component::Dialog::New(cx)
                                   ->Open(true)
                                   ->Overlay(self->overlay)
                                   ->OverlayClosable(self->overlayClosable)
                                   ->OnClose(close)
                                   ->OnCancel(close)
                                   ->OnOk(close);
        switch (self->open) {
            case DlgDefault: {
                El* content = Div(a)->FlexCol()->W(kFill);
                content->Child(
                    DialogHeader(
                        cx, StrL("Basic Dialog"),
                        StrL("This is a basic dialog created using the "
                             "declarative API."))
                        ->Pad(16));
                El* body =
                    Div(a)->FlexCol()->W(kFill)->Gap(12)->PadX(16)->PadB(16);
                body->Child(
                    StoryTxt(cx,
                             StrL("This is a dialog dialog, you can put "
                                  "anything here."),
                             16, th.foreground));
                body->Child(component::Input::New(cx, StrL("dialog-name"),
                                                  &self->basicInput)
                                ->OnFocus(Listen(cx, &FocusBasicInput))
                                ->IntoEl());
                body->Child(component::Select::New(cx, StrL("dialog-select"),
                                                   self->basicSelect)
                                ->Items(gDialogOptions, 3)
                                ->Placeholder(StrL("Select an option"))
                                ->W(kFill)
                                ->OnToggle(Listen(cx, &ToggleBasicSelect))
                                ->IntoEl());
                body->Child(component::DatePicker::New(cx)
                                ->Year(self->basicDate.year)
                                ->Month(self->basicDate.month)
                                ->Day(self->basicDate.day)
                                ->Placeholder(StrL("Date of Birth"))
                                ->W(kFill)
                                ->Open(self->basicDateOpen)
                                ->OnToggle(Listen(cx, &ToggleBasicDate))
                                ->OnDay(Listen(cx, &PickBasicDate))
                                ->IntoEl());
                content->Child(body);

                El* actions = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
                actions->Child(DialogButton(cx, StrL("dialog-cancel"),
                                            StrL("Cancel"), close, false));
                actions->Child(DialogButton(cx, StrL("dialog-confirm"),
                                            StrL("Confirm"), close, true));
                El* footer = Div(a)
                                 ->FlexRow()
                                 ->W(kFill)
                                 ->Pad(16)
                                 ->Gap(8)
                                 ->ItemsCenter()
                                 ->JustifyBetween()
                                 ->Bg(th.muted);
                footer->Child(DialogButton(cx, StrL("new-dialog"),
                                           StrL("Open Other Dialog"),
                                           ListenerArg(open, DlgOther), false));
                footer->Child(actions);
                content->Child(footer);
                d->CloseButton(self->closeButton)->Surface(content);
                break;
            }
            case DlgCustomButtons: {
                El* surface = Div(a)->FlexCol()->W(kFill)->Gap(16)->Pad(16);
                El* body = Div(a)->FlexCol()->W(kFill)->Gap(12)->ItemsCenter();
                body->Child(Div(a)
                                ->W(48)
                                ->H(48)
                                ->ItemsCenter()
                                ->JustifyCenter()
                                ->Radius(th.radiusLg)
                                ->Bg(RgbaOpacity(th.warning, 0.2f))
                                ->Child(IconEl(a, IconName::TriangleAlert, 32)
                                            ->Fg(th.warning)));
                body->Child(
                    StoryTxt(cx,
                             StrL("Update successful, we need to restart the "
                                  "application."),
                             16, th.foreground));
                surface->Child(body);
                El* footer = Div(a)->FlexRow()->W(kFill)->Gap(8)->JustifyEnd();
                footer->Child(DialogButton(cx, StrL("later"), StrL("Later"),
                                           close, false));
                footer->Child(DialogButton(cx, StrL("restart"),
                                           StrL("Restart Now"), close, true));
                surface->Child(footer);
                d->CloseButton()->Surface(surface);
                break;
            }
            case DlgScrollable: {
                TempStr markdown = AssetsLoadTextTemp(StrL("story/README.md"));
                Str source = markdown.s ? Str(markdown.s)
                                        : StrL("# README.md is missing");
                El* surface =
                    Div(a)->FlexCol()->W(kFill)->H(kFill)->Gap(16)->Pad(16);
                surface->Child(DialogTitleText(
                    cx, StrL("Dialog with scrollbar"), th.foreground));
                surface->Child(
                    component::Scrollable::New(cx, StrL("dialog-scroll"))
                        ->H(484)
                        ->ScrollY(self->dialogScrollY)
                        ->OnScroll(Listen(cx, &OnDialogScroll))
                        ->Child(component::TextView::New(cx, source)->IntoEl())
                        ->IntoEl());
                El* footer = Div(a)->FlexRow()->W(kFill)->Gap(8)->JustifyEnd();
                footer->Child(DialogButton(cx, StrL("scroll-cancel"),
                                           StrL("Cancel"), close, false));
                footer->Child(DialogButton(cx, StrL("scroll-confirm"),
                                           StrL("Confirm"), close, true));
                surface->Child(footer);
                d->W(720)->H(600)->CloseButton()->Surface(surface);
                break;
            }
            case DlgTable: {
                static const component::TableColumn kColumns[] = {
                    {StrL("ID"), 50, false, false, true},
                    {StrL("Name"), 150, false, false, true},
                    {StrL("Email"), 250, false, false, true},
                    {StrL("Role"), 150, false, false, true},
                    {StrL("Status"), 100, false, false, true},
                };
                El* surface =
                    Div(a)->FlexCol()->W(kFill)->H(kFill)->Gap(16)->Pad(16);
                surface->Child(DialogTitleText(cx, StrL("Dialog with Table"),
                                               th.foreground));
                El* body = Div(a)->FlexCol()->W(kFill)->Gap(12);
                body->Child(StoryTxt(
                    cx, StrL("This is a dialog contains a table component."),
                    16, th.foreground));
                body->Child(component::DataTable::New(cx, StrL("dialog-table"),
                                                      self->table)
                                ->Columns(kColumns, 5)
                                ->Rows(200, self, DialogTableCell)
                                ->H(430)
                                ->IntoEl());
                surface->Child(body);
                d->W(800)->H(600)->CloseButton()->Surface(surface);
                break;
            }
            case DlgNoTitle: {
                El* surface = Div(a)->W(kFill)->Pad(16)->Child(
                    StoryTxt(cx,
                             StrL("This is a dialog without title, you can use "
                                  "it when the title is not necessary."),
                             16, th.foreground)
                        ->Wrap()
                        ->W(kFill));
                d->CloseButton()->Surface(surface);
                break;
            }
            case DlgPadding: {
                El* surface = Div(a)->FlexCol()->W(kFill)->Gap(12)->Pad(12);
                surface->Child(DialogTitleText(cx, StrL("Custom Dialog Title"),
                                               th.foreground));
                surface->Child(
                    StoryTxt(cx,
                             StrL("This is a custom dialog content, we can use "
                                  "paddings to control the layout and spacing "
                                  "within the dialog."),
                             16, th.foreground)
                        ->Wrap()
                        ->W(kFill));
                d->Overlay(true)->OverlayClosable(true)->CloseButton()->Surface(
                    surface);
                break;
            }
            case DlgStyle: {
                El* surface = Div(a)->FlexCol()->W(kFill)->Gap(16)->Pad(16);
                surface->Child(DialogTitleText(cx, StrL("Custom Dialog Title"),
                                               th.infoFg));
                surface->Child(
                    StoryTxt(cx, StrL("This is a custom dialog content."), 16,
                             th.infoFg));
                d->Overlay(true)
                    ->OverlayClosable(true)
                    ->CloseButton()
                    ->Radius(th.radiusLg)
                    ->Bg(th.cyan)
                    ->Fg(th.infoFg)
                    ->Surface(surface);
                break;
            }
            case DlgContent: {
                El* surface = Div(a)->FlexCol()->W(kFill)->Gap(16)->Pad(16);
                surface->Child(DialogHeader(
                    cx, StrL("Custom Width"),
                    StrL("This dialog has a custom width of 400px.")));
                surface->Child(
                    StoryTxt(cx,
                             StrL("Content area with custom width "
                                  "configuration, and the footer is used flex "
                                  "1 button widths."),
                             16, th.foreground)
                        ->Wrap()
                        ->W(kFill));
                El* footer =
                    Div(a)->FlexRow()->W(kFill)->Gap(8)->JustifyCenter();
                footer->Child(DialogButton(cx, StrL("content-cancel"),
                                           StrL("Cancel"), close, false)
                                  ->Grow());
                footer->Child(DialogButton(cx, StrL("content-done"),
                                           StrL("Done"), close, true)
                                  ->Grow());
                surface->Child(footer);
                d->Overlay(true)
                    ->OverlayClosable(true)
                    ->W(400)
                    ->CloseButton()
                    ->Surface(surface);
                break;
            }
            case DlgTextView: {
                El* surface = Div(a)->FlexCol()->W(kFill);
                surface->Child(Div(a)->W(kFill)->Pad(16)->Child(DialogTitleText(
                    cx, StrL("TextView Dialog"), th.foreground)));
                surface->Child(Div(a)->W(kFill)->PadX(16)->PadB(16)->Child(
                    component::TextView::New(
                        cx, StrL("This is a dialog with a selectable TextView "
                                 "in it. This text should be selectable."))
                        ->Selectable()
                        ->IntoEl()));
                El* footer = Div(a)
                                 ->FlexRow()
                                 ->W(kFill)
                                 ->Pad(16)
                                 ->Gap(8)
                                 ->JustifyEnd()
                                 ->Bg(th.muted);
                footer->Child(DialogButton(cx, StrL("text-cancel"),
                                           StrL("Cancel"), close, false));
                footer->Child(DialogButton(cx, StrL("text-confirm"),
                                           StrL("Confirm"), close, true));
                surface->Child(footer);
                d->CloseButton(self->closeButton)->Surface(surface);
                break;
            }
            default: {
                El* surface = Div(a)->FlexCol()->W(kFill)->Gap(16)->Pad(16);
                surface->Child(
                    DialogTitleText(cx, StrL("Other Dialog"), th.foreground));
                surface->Child(StoryTxt(cx, StrL("This is another dialog."), 16,
                                        th.foreground));
                d->Overlay(true)
                    ->OverlayClosable(self->overlayClosable)
                    ->CloseButton()
                    ->Surface(surface);
                break;
            }
        }
        page->Child(d->IntoEl(size));
    }
    return page;
}

// crates/base/src/dialog.rs binds escape to Cancel and enter to Confirm, both
// inside a key context that only exists while `keyboard` is on. Either one
// closes the dialog here; what separates them is the reason it closed, which
// is what Rust reports beside the new open state.
void DialogStory::OnKey(DialogStory* self, Ctx* cx, const KeyEvent* ev) {
    if (!ev->down || self->open < 0) {
        return;
    }
    // The toolbar option belongs to the two declarative dialogs that capture
    // it. Imperative open_dialog calls keep Dialog's default keyboard support.
    bool keyboard = self->open == DlgDefault || self->open == DlgTextView
                        ? self->keyboard
                        : true;
    DialogAction act = DialogActionForKey(ev->vk, keyboard);
    if (act == DialogAction::None) {
        return;
    }
    // Rust reports DialogChangeReason::Confirm or ::Cancel with the close;
    // this page does the same thing either way, so it only needs which key.
    self->open = -1;
    if (act == DialogAction::Confirm) {
        // cx.stop_propagation(): the Enter was the dialog's, so it must not
        // also activate whatever element still holds focus behind it.
        cx->win->eatReturn = true;
    }
    Notify(cx);
}

STORY_PAGE_KEYS(StoryDialog, DialogStory);
