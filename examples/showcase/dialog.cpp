#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickDlgOpen = 350,
    ClickDlgCancel = 351,
    ClickDlgSave = 352,
    ClickDlgField = 353
};

El* ShowcaseDialog(ShowcaseApp* app, Arena* a) {
    El* root = Div(a)->FlexCol();
    root->Child(Button::New(a, StrL("open-dialog"), ClickDlgOpen)
                    ->H(28)
                    ->PadX(12)
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Bg(Rgb(0, 0, 0))
                    ->Child(TextEl(a, StrL("Edit profile"))
                                ->Font(12)
                                ->Fg(Rgb(0xff, 0xff, 0xff))));
    if (!app->dialogOpen) {
        return root;
    }
    El* panel =
        Div(a)
            ->W(288)
            ->Pad(12)
            ->FlexCol()
            ->Bg(Rgb(0xff, 0xff, 0xff))
            ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
            ->Child(DialogTitle::New(a)->Child(TextEl(a, StrL("Edit profile"))
                                                   ->Font(12)
                                                   ->Fg(Rgb(0x17, 0x17, 0x17))
                                                   ->Semibold()))
            ->Child(DialogDescription::New(a)->Child(
                TextEl(a,
                       StrL("Update the public details shown on your profile."))
                    ->Font(12)
                    ->Fg(Rgb(0x73, 0x73, 0x73))
                    ->Wrap()
                    ->MaxW(264)))
            ->Child(Div(a)->PadT(12)->Child(TextEl(a, StrL("Display name"))
                                                ->Font(14)
                                                ->Fg(Rgb(0x17, 0x17, 0x17))))
            ->Child(InputBase::New(a, StrL("dialog-name"), ClickDlgField)
                        ->W(264)
                        ->H(28)
                        ->PadX(8)
                        ->ItemsCenter()
                        ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
                        ->Child(Input::New(a, &app->input)))
            ->Child(
                Div(a)
                    ->PadT(12)
                    ->FlexRow()
                    ->JustifyEnd()
                    ->Gap(8)
                    ->Child(DialogClose::New(a, ClickDlgCancel)
                                ->Child(Button::New(a, StrL("dialog-cancel"),
                                                    ClickDlgCancel)
                                            ->H(28)
                                            ->PadX(12)
                                            ->ItemsCenter()
                                            ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
                                            ->Child(TextEl(a, StrL("Cancel"))
                                                        ->Font(12)
                                                        ->Fg(Rgb(0x17, 0x17,
                                                                 0x17)))))
                    ->Child(Button::New(a, StrL("dialog-save"), ClickDlgSave)
                                ->H(28)
                                ->PadX(12)
                                ->ItemsCenter()
                                ->Bg(Rgb(0x17, 0x17, 0x17))
                                ->Child(TextEl(a, StrL("Save changes"))
                                            ->Font(12)
                                            ->Fg(Rgb(0xff, 0xff, 0xff)))));
    El* backdrop = DialogBackdrop::New(a)
                       ->Absolute()
                       ->Top(0)
                       ->Left(0)
                       ->W(kFill)
                       ->H(kFill)
                       ->Bg(Rgba8(0, 0, 0, 51))
                       ->Click(ClickDlgCancel);
    El* popup = DialogPopup::New(a)
                    ->Absolute()
                    ->Top(0)
                    ->Left(0)
                    ->W(kFill)
                    ->H(kFill)
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Child(panel);
    root->Child(Dialog::New(a)->Backdrop(backdrop)->Popup(popup)->IntoEl());
    return root;
}

void ShowcaseDialogClick(ShowcaseApp* app, int id) {
    if (id == ClickDlgOpen) {
        app->dialogOpen = true;
        app->input.focused = true;
    } else if (id == ClickDlgCancel || id == ClickDlgSave) {
        app->dialogOpen = false;
        app->input.focused = false;
    } else if (id == ClickDlgField) {
        app->input.focused = true;
    }
}

SHOWCASE_PAGE(CompDialog, ShowcaseDialog, ShowcaseDialogClick);
