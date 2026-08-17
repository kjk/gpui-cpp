#include "Story.h"

enum {
    ClickAlertBannerClose = 2200
};

static void HideBanner(StoryApp* app) {
    app->alertBanner = false;
}
static void AlertNoop(StoryApp*) {}

El* AlertRender(StoryApp* app, Arena* a) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));

    El* def = StorySection(a, "Default", "Title, icon, and rich text content.");
    StorySectionAdd(
        def, component::Alert::New(a, StrL("alert-default"),
                                   StrL("Your workspace is ready for the team. "
                                        "12 members have access."))
                 ->Title(StrL("Workspace settings saved"))
                 ->WithSize(app->size)
                 ->IntoEl());
    page->Child(def);

    El* vars = StorySection(a, "Variants",
                            "Info, success, warning, and error states.");
    El* col = Div(a)->FlexCol()->Gap(12)->W(kFill);
    col->Child(
        component::Alert::Info(a, StrL("info1"),
                               StrL("Maintenance starts Friday at 22:00 UTC."))
            ->Title(StrL("Scheduled maintenance"))
            ->OnClose(MkFunc0(&AlertNoop, app))
            ->WithSize(app->size)
            ->IntoEl());
    col->Child(
        component::Alert::Success(a, StrL("success-1"),
                                  StrL("The transfer is queued and usually "
                                       "settles within one business day."))
            ->Title(StrL("Transfer submitted"))
            ->WithSize(app->size)
            ->IntoEl());
    col->Child(
        component::Alert::Warning(
            a, StrL("warning-1"),
            StrL("Two teammates still use recovery codes generated more than "
                 "a year ago. Ask them to generate a fresh set in Security "
                 "settings."))
            ->WithSize(app->size)
            ->IntoEl());
    col->Child(
        component::Alert::Error(
            a, StrL("error-1"),
            StrL("Please verify your billing information and try again. Check "
                 "your card details, ensure sufficient funds, and verify the "
                 "billing address."))
            ->Title(StrL("Unable to process your payment."))
            ->WithSize(app->size)
            ->IntoEl());
    StorySectionAdd(vars, col);
    page->Child(vars);

    El* ban = StorySection(a, "Banner", "Full-width and closable alerts.");
    El* bcol = Div(a)->FlexCol()->Gap(8)->W(kFill);
    if (app->alertBanner) {
        bcol->Child(
            component::Alert::New(
                a, StrL("banner-1"),
                StrL("Reporting is read-only while the nightly ledger closes."))
                ->Banner()
                ->Visible(true)
                ->OnClose(MkFunc0(&HideBanner, app))
                ->WithSize(app->size)
                ->IntoEl());
    }
    bcol->Child(
        component::Alert::Info(
            a, StrL("banner-info"),
            StrL("A new desktop update will install after you restart."))
            ->Banner()
            ->WithSize(app->size)
            ->IntoEl());
    bcol->Child(
        component::Alert::Success(a, StrL("banner-success"),
                                  StrL("All 1,284 records finished importing."))
            ->Banner()
            ->WithSize(app->size)
            ->IntoEl());
    bcol->Child(component::Alert::Warning(
                    a, StrL("banner-warning"),
                    StrL("Your API key expires in 6 days. Rotate it before "
                         "August 19."))
                    ->Banner()
                    ->WithSize(app->size)
                    ->IntoEl());
    bcol->Child(component::Alert::Error(
                    a, StrL("banner-error"),
                    StrL("Live updates are disconnected. Changes may be "
                         "delayed."))
                    ->Banner()
                    ->WithSize(app->size)
                    ->IntoEl());
    StorySectionAdd(ban, bcol);
    page->Child(ban);
    return page;
}

void AlertClick(StoryApp* app, int id) {
    if (id == ClickAlertBannerClose) {
        app->alertBanner = false;
    }
}

STORY_PAGE(StoryAlert, AlertRender, AlertClick);
