#include "Showcase.h"
#include "ui/AlertDialog.h"
#include "ui/Button.h"

enum {
    ClickAlertOpen = 210,
    ClickAlertCancel = 211,
    ClickAlertOk = 212
};

El* ShowcaseAlertDialog(ShowcaseApp* app, Arena* a) {
    El* root = Div(a)->FlexCol();
    root->Child(Button::New(a, StrL("open-alert-dialog"), ClickAlertOpen)
                    ->H(28)
                    ->PadX(12)
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Bg(Rgb(0, 0, 0))
                    ->Child(TextEl(a, StrL("Delete project"))
                                ->Font(12)
                                ->Fg(Rgb(0xff, 0xff, 0xff))));
    if (!app->alertOpen) {
        return root;
    }

    El* panel =
        Div(a)
            ->W(288)
            ->Pad(12)
            ->FlexCol()
            ->Bg(Rgb(0xff, 0xff, 0xff))
            ->Border(1, Rgb(0x17, 0x17, 0x17))
            ->Child(AlertDialogTitle::New(a)
                        ->Child(TextEl(a, StrL("Delete project?"))
                                    ->Font(14)
                                    ->Fg(Rgb(0x17, 0x17, 0x17))))
            ->Child(Div(a)->H(8))
            ->Child(AlertDialogDescription::New(a)
                        ->Child(TextEl(a, StrL("This permanently deletes Acme "
                                               "Studio and all of its data."))
                                    ->Font(12)
                                    ->Fg(Rgb(0x52, 0x52, 0x52))
                                    ->Wrap()
                                    ->MaxW(264)))
            ->Child(Div(a)->H(12))
            ->Child(
                Div(a)
                    ->FlexRow()
                    ->W(kFill)
                    ->JustifyEnd()
                    ->Gap(8)
                    ->Child(AlertDialogCancel::New(a)->Child(
                        Button::New(a, StrL("cancel-delete"), ClickAlertCancel)
                            ->H(28)
                            ->PadX(12)
                            ->ItemsCenter()
                            ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
                            ->Child(TextEl(a, StrL("Cancel"))
                                        ->Font(12)
                                        ->Fg(Rgb(0x17, 0x17, 0x17)))))
                    ->Child(AlertDialogAction::New(a)->Child(
                        Button::New(a, StrL("confirm-delete"), ClickAlertOk)
                            ->H(28)
                            ->PadX(12)
                            ->ItemsCenter()
                            ->Border(1, Rgb(0x17, 0x17, 0x17))
                            ->Bg(Rgb(0x17, 0x17, 0x17))
                            ->Child(TextEl(a, StrL("Delete"))
                                        ->Font(12)
                                        ->Fg(Rgb(0xff, 0xff, 0xff))))));

    El* backdrop = AlertDialogBackdrop::New(a)
                       ->Absolute()
                       ->Top(0)
                       ->Left(0)
                       ->W(kFill)
                       ->H(kFill)
                       ->Bg(Rgba8(0, 0, 0, 46))
                       ->Click(ClickAlertCancel);
    // Rust AlertDialogPopup is flex items/justify center with no inset_0,
    // so it sits at the top of the viewport host (not vertically centered).
    El* popup = AlertDialogPopup::New(a)
                    ->W(kFill)
                    ->FlexRow()
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Child(panel);
    root->Child(
        AlertDialog::New(a)->Backdrop(backdrop)->Popup(popup)->IntoEl());
    return root;
}

void ShowcaseAlertDialogClick(ShowcaseApp* app, int id) {
    if (id == ClickAlertOpen) {
        app->alertOpen = true;
    } else if (id == ClickAlertCancel || id == ClickAlertOk) {
        app->alertOpen = false;
    }
}

SHOWCASE_PAGE(CompAlertDialog, ShowcaseAlertDialog, ShowcaseAlertDialogClick);
