#include "Story.h"

// One entry per section, in the order the Rust story renders them.
enum {
    AlertDefault = 0,
    AlertImperative,
    AlertIcon,
    AlertDestructive,
    AlertNoTitle,
    AlertCustomFooter,
    AlertCustomContent,
    AlertKeyboard,
    AlertConfirm,
    AlertPreventClose,
    AlertCount
};

struct AlertDialogStory {
    int open = -1;

    static El* Render(AlertDialogStory* self, Ctx* cx);
    static void OnKey(AlertDialogStory* self, Ctx* cx, const KeyEvent* ev);
};

static void OpenAlert(AlertDialogStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t which) {
    self->open = (int)which;
    Notify(cx);
}
static void CloseAlert(AlertDialogStory* self, Ctx* cx, const ClickEvent*) {
    self->open = -1;
    Notify(cx);
}

struct AlertSpec {
    int which;
    const char* title;
    const char* description;
    const char* id;
    const char* label;
    bool danger;
    const char* dialogTitle;
    const char* dialogBody;
};

static const AlertSpec kAlerts[] = {
    {AlertDefault, "Default",
     "Compose the header, message, and footer "
     "actions.",
     "info-alert", "Discard Draft", false, "Discard this draft?",
     "Your changes will be lost if you leave without saving."},
    {AlertImperative, "Imperative API",
     "Open an alert directly from the "
     "window.",
     "confirm-alert", "Delete File", false, "Delete this file?",
     "The file will be moved to the trash."},
    {AlertIcon, "Icon", "Add a visual cue above the title.", "icon-alert",
     "Request Permission", false, "Allow access?",
     "This app would like to use your microphone."},
    {AlertDestructive, "Destructive",
     "Use a destructive action for "
     "irreversible choices.",
     "destructive-action", "Delete Account", true, "Delete your account?",
     "This permanently removes your account and all of its data."},
    {AlertNoTitle, "Without title", "Render content without a heading.",
     "without-title", "Continue without Title", false, nullptr,
     "Continue without a title on this confirm?"},
    {AlertCustomFooter, "Custom footer", "Replace the default action row.",
     "session-timeout", "Show Session Expiry", false, "Session expiring",
     "You will be signed out in five minutes."},
    {AlertCustomContent, "Custom content",
     "Style header and footer regions independently.", "update",
     "Install Update", false, "Update available",
     "Version 2.1 is ready to install."},
    {AlertKeyboard, "Keyboard", "Disable keyboard dismissal when required.",
     "keyboard-disabled", "Review Notice", false, "Review this notice",
     "Press the button below to acknowledge."},
    {AlertConfirm, "Confirm mode", "Provide standard OK and Cancel actions.",
     "overlay-closable", "Open Confirmation", false, "Are you sure?",
     "This action cannot be undone."},
    {AlertPreventClose, "Prevent close",
     "Callbacks can keep the dialog "
     "open.",
     "prevent-close", "Close During Sync", false, "Sync in progress",
     "Closing now would leave the workspace half-synced."},
};

El* AlertDialogStory::Render(AlertDialogStory* self, Ctx* cx) {
    WinSize size = WindowSize(cx->win);
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    Listener open = Listen(cx, &OpenAlert);

    for (size_t i = 0; i < sizeof(kAlerts) / sizeof(kAlerts[0]); i++) {
        const AlertSpec& s = kAlerts[i];
        El* sec = StorySection(cx, s.title, s.description);
        component::Button* btn = component::Button::New(cx, Str(s.id))
                                     ->Label(Str(s.label))
                                     ->Outline()
                                     ->OnClick(ListenerArg(open, s.which));
        if (s.danger) {
            btn->Danger();
        }
        StorySectionAdd(sec, btn->IntoEl());
        page->Child(sec);
    }

    if (self->open >= 0) {
        const AlertSpec& s = kAlerts[self->open];
        component::Dialog* d = component::Dialog::New(cx)
                                   ->Open(true)
                                   ->OnClose(Listen(cx, &CloseAlert))
                                   ->OnOk(Listen(cx, &CloseAlert));
        if (s.dialogTitle) {
            d->Title(Str(s.dialogTitle));
        }
        d->Description(Str(s.dialogBody));
        page->Child(d->IntoEl(size));
    }
    return page;
}

// Esc closes what this page has open, like an overlay dismiss.
void AlertDialogStory::OnKey(AlertDialogStory* self, Ctx* cx,
                             const KeyEvent* ev) {
    if (ev->vk != KeyEscape) {
        return;
    }
    self->open = -1;
    Notify(cx);
}

STORY_PAGE_KEYS(StoryAlertDialog, AlertDialogStory);
