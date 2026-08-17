#include "Story.h"

enum {
    ClickAlertBannerClose = 2200
};

static void HideBanner(StoryApp* app) {
    app->alertBanner = false;
}
static void AlertNoop(StoryApp*) {}

static El* AlertLine(Arena* a, Str s, Rgba fg) {
    return TextEl(a, s)->Font(14)->Fg(fg)->Wrap();
}

static El* AlertBullet(Arena* a, Rgba fg, El* text) {
    El* row = Div(a)->FlexRow()->Gap(8)->ItemsStart();
    row->Child(AlertLine(a, StrL("\xE2\x80\xA2"), fg)->Shrink0());
    row->Child(text);
    return row;
}

static El* AlertW(Arena* a, El* child) {
    // Rust Alert sections use .w_2_3() on the inner pane (~640px).
    return Div(a)->W(640)->Child(child);
}

El* AlertRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));

    El* defBody = Div(a)->FlexCol()->Gap(4);
    defBody->Child(AlertLine(a, StrL("Your workspace is ready for the team."),
                             th.foreground));
    El* members = Div(a)->FlexRow()->Gap(4)->Wrap();
    members->Child(AlertLine(a, StrL("12 members"), th.foreground)->Semibold());
    members->Child(AlertLine(a, StrL("have access"), th.foreground));
    defBody->Child(AlertBullet(a, th.foreground, members));
    defBody->Child(AlertBullet(
        a, th.foreground,
        AlertLine(a, StrL("Billing remains with the workspace owner"),
                  th.foreground)));

    El* def = StorySection(a, "Default", "Title, icon, and rich text content.");
    StorySectionAdd(
        def, AlertW(a, component::Alert::New(a, StrL("alert-default"), Str{})
                           ->Title(StrL("Workspace settings saved"))
                           ->Content(defBody)
                           ->WithSize(app->size)
                           ->IntoEl()));
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

    El* warnBody = Div(a)->FlexCol()->Gap(2);
    warnBody->Child(AlertLine(
        a,
        StrL("Two teammates still use recovery codes generated more than a "
             "year ago."),
        th.warning));
    warnBody->Child(AlertLine(
        a, StrL("Ask them to generate a fresh set in Security settings."),
        th.warning));
    col->Child(component::Alert::Warning(a, StrL("warning-1"), Str{})
                   ->Content(warnBody)
                   ->WithSize(app->size)
                   ->IntoEl());

    El* errBody = Div(a)->FlexCol()->Gap(4);
    errBody->Child(AlertLine(
        a, StrL("Please verify your billing information and try again."),
        th.danger));
    const char* errItems[] = {"Check your card details",
                              "Ensure sufficient funds",
                              "Verify billing address"};
    for (int i = 0; i < 3; i++) {
        errBody->Child(AlertBullet(a, th.danger,
                                   AlertLine(a, Str(errItems[i]), th.danger)));
    }
    col->Child(component::Alert::Error(a, StrL("error-1"), Str{})
                   ->Title(StrL("Unable to process your payment."))
                   ->Content(errBody)
                   ->WithSize(app->size)
                   ->IntoEl());
    StorySectionAdd(vars, AlertW(a, col));
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
    StorySectionAdd(ban, AlertW(a, bcol));
    page->Child(ban);

    El* custom =
        StorySection(a, "Custom icon", "Custom icon and long content.");
    StorySectionAdd(
        custom,
        AlertW(a, component::Alert::New(
                      a, StrL("other-1"),
                      StrL("The quarterly planning review overlaps with the "
                           "APAC operations call. Move one event or invite "
                           "another owner before sending the agenda."))
                      ->Title(StrL("Two events overlap by 30 minutes"))
                      ->Icon(IconName::Calendar)
                      ->WithSize(app->size)
                      ->IntoEl()));
    page->Child(custom);
    return page;
}

void AlertClick(StoryApp* app, int id) {
    if (id == ClickAlertBannerClose) {
        app->alertBanner = false;
    }
}

STORY_PAGE(StoryAlert, AlertRender, AlertClick);
