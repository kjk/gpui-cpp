#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

static void OpenAlert(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    app->alertOpen = true;
    Notify(cx);
}

static void CloseAlert(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    app->alertOpen = false;
    Notify(cx);
}

El* ShowcaseAlertDialog(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* root = Div(a)->FlexCol();
    root->Child(Button::New(cx, StrL("open-alert-dialog"))
                    ->OnClick(Listen(cx, &OpenAlert))
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
            ->Child(AlertDialogTitle::New(cx)
                        ->Child(TextEl(a, StrL("Delete project?"))
                                    ->Font(14)
                                    ->Fg(Rgb(0x17, 0x17, 0x17))))
            ->Child(Div(a)->H(8))
            ->Child(AlertDialogDescription::New(cx)
                        ->Child(TextEl(a, StrL("This permanently deletes Acme "
                                               "Studio and all of its data."))
                                    ->Font(12)
                                    ->Fg(Rgb(0x52, 0x52, 0x52))
                                    ->Wrap()
                                    ->MaxW(264)))
            ->Child(Div(a)->H(12))
            ->Child(Div(a)
                        ->FlexRow()
                        ->W(kFill)
                        ->JustifyEnd()
                        ->Gap(8)
                        ->Child(AlertDialogCancel::New(cx)->Child(
                            Button::New(cx, StrL("cancel-delete"))
                                ->H(28)
                                ->PadX(12)
                                ->ItemsCenter()
                                ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
                                ->Child(TextEl(a, StrL("Cancel"))
                                            ->Font(12)
                                            ->Fg(Rgb(0x17, 0x17, 0x17)))))
                        ->Child(AlertDialogAction::New(cx)->Child(
                            Button::New(cx, StrL("confirm-delete"))
                                ->H(28)
                                ->PadX(12)
                                ->ItemsCenter()
                                ->Border(1, Rgb(0x17, 0x17, 0x17))
                                ->Bg(Rgb(0x17, 0x17, 0x17))
                                ->Child(TextEl(a, StrL("Delete"))
                                            ->Font(12)
                                            ->Fg(Rgb(0xff, 0xff, 0xff))))));

    El* backdrop = AlertDialogBackdrop::New(cx)
                       ->Absolute()
                       ->Top(0)
                       ->Left(0)
                       ->W(kFill)
                       ->H(kFill)
                       ->Bg(Rgba8(0, 0, 0, 46))
                       ->Click(HashClickId(StrL("alert-backdrop")));
    // Rust AlertDialogPopup is flex items/justify center with no inset_0,
    // so it sits at the top of the viewport host (not vertically centered).
    El* popup = AlertDialogPopup::New(cx)
                    ->W(kFill)
                    ->FlexRow()
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Child(panel);
    // dialog.rs handles the alert's keyboard too — an alert has no bindings
    // of its own, it rides the Dialog context — and the two buttons dispatch
    // Cancel and Confirm rather than carrying a handler each, so this is the
    // one place the page says what they do.
    DialogBindKeys(cx, popup, StrL("showcase-alert"), Listen(cx, &CloseAlert),
                   Listen(cx, &CloseAlert), {});
    root->Child(
        AlertDialog::New(cx)->Backdrop(backdrop)->Popup(popup)->IntoEl());
    return root;
}

SHOWCASE_PAGE(CompAlertDialog, ShowcaseAlertDialog);
