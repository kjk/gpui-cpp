#include "Story.h"

struct AlertStory {
    bool alertBanner = true;
    StoryToolbarState toolbar;

    static El* Render(AlertStory* self, Ctx* cx);
    static void Click(AlertStory* self, Ctx* cx, int id);
};

enum {
    ClickAlertBannerClose = 2200
};

static void HideBanner(AlertStory* self) {
    self->alertBanner = false;
}
static void AlertNoop(AlertStory*) {}

static El* AlertLine(Ctx* cx, Str s, Rgba fg) {
    Arena* a = cx->a;
    return TextEl(a, s)->Font(14)->Fg(fg)->Wrap();
}

static El* AlertBullet(Ctx* cx, Rgba fg, El* text) {
    Arena* a = cx->a;
    El* row = Div(a)->FlexRow()->Gap(8)->ItemsStart();
    row->Child(AlertLine(cx, StrL("\xE2\x80\xA2"), fg)->Shrink0());
    row->Child(text);
    return row;
}

static El* AlertW(Ctx* cx, El* child) {
    Arena* a = cx->a;
    // Rust Alert sections use .w_2_3() on the inner pane (~640px).
    return Div(a)->W(640)->Child(child);
}

El* AlertStory::Render(AlertStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, &self->toolbar));

    El* defBody = Div(a)->FlexCol()->Gap(4);
    defBody->Child(AlertLine(cx, StrL("Your workspace is ready for the team."),
                             th.foreground));
    El* members = Div(a)->FlexRow()->Gap(4)->Wrap();
    members
        ->Child(AlertLine(cx, StrL("12 members"), th.foreground)->Semibold());
    members->Child(AlertLine(cx, StrL("have access"), th.foreground));
    defBody->Child(AlertBullet(cx, th.foreground, members));
    defBody->Child(AlertBullet(
        cx, th.foreground,
        AlertLine(cx, StrL("Billing remains with the workspace owner"),
                  th.foreground)));

    El* def =
        StorySection(cx, "Default", "Title, icon, and rich text content.");
    StorySectionAdd(
        def, AlertW(cx, component::Alert::New(cx, StrL("alert-default"), Str{})
                            ->Title(StrL("Workspace settings saved"))
                            ->Content(defBody)
                            ->WithSize(self->toolbar.size)
                            ->IntoEl()));
    page->Child(def);

    El* vars = StorySection(cx, "Variants",
                            "Info, success, warning, and error states.");
    El* col = Div(a)->FlexCol()->Gap(12)->W(kFill);
    col->Child(
        component::Alert::Info(cx, StrL("info1"),
                               StrL("Maintenance starts Friday at 22:00 UTC."))
            ->Title(StrL("Scheduled maintenance"))
            ->OnClose(MkFunc0(&AlertNoop, self))
            ->WithSize(self->toolbar.size)
            ->IntoEl());
    col->Child(
        component::Alert::Success(cx, StrL("success-1"),
                                  StrL("The transfer is queued and usually "
                                       "settles within one business day."))
            ->Title(StrL("Transfer submitted"))
            ->WithSize(self->toolbar.size)
            ->IntoEl());

    El* warnBody = Div(a)->FlexCol()->Gap(2);
    warnBody->Child(AlertLine(
        cx,
        StrL("Two teammates still use recovery codes generated more than a "
             "year ago."),
        th.warning));
    warnBody->Child(AlertLine(
        cx, StrL("Ask them to generate a fresh set in Security settings."),
        th.warning));
    col->Child(component::Alert::Warning(cx, StrL("warning-1"), Str{})
                   ->Content(warnBody)
                   ->WithSize(self->toolbar.size)
                   ->IntoEl());

    El* errBody = Div(a)->FlexCol()->Gap(4);
    errBody->Child(AlertLine(
        cx, StrL("Please verify your billing information and try again."),
        th.danger));
    const char* errItems[] = {"Check your card details",
                              "Ensure sufficient funds",
                              "Verify billing address"};
    for (int i = 0; i < 3; i++) {
        errBody->Child(AlertBullet(cx, th.danger,
                                   AlertLine(cx, Str(errItems[i]), th.danger)));
    }
    col->Child(component::Alert::Error(cx, StrL("error-1"), Str{})
                   ->Title(StrL("Unable to process your payment."))
                   ->Content(errBody)
                   ->WithSize(self->toolbar.size)
                   ->IntoEl());
    StorySectionAdd(vars, AlertW(cx, col));
    page->Child(vars);

    El* ban = StorySection(cx, "Banner", "Full-width and closable alerts.");
    El* bcol = Div(a)->FlexCol()->Gap(8)->W(kFill);
    if (self->alertBanner) {
        bcol->Child(
            component::Alert::New(
                cx, StrL("banner-1"),
                StrL("Reporting is read-only while the nightly ledger closes."))
                ->Banner()
                ->Visible(true)
                ->OnClose(MkFunc0(&HideBanner, self))
                ->WithSize(self->toolbar.size)
                ->IntoEl());
    }
    bcol->Child(
        component::Alert::Info(
            cx, StrL("banner-info"),
            StrL("A new desktop update will install after you restart."))
            ->Banner()
            ->WithSize(self->toolbar.size)
            ->IntoEl());
    bcol->Child(
        component::Alert::Success(cx, StrL("banner-success"),
                                  StrL("All 1,284 records finished importing."))
            ->Banner()
            ->WithSize(self->toolbar.size)
            ->IntoEl());
    bcol->Child(component::Alert::Warning(
                    cx, StrL("banner-warning"),
                    StrL("Your API key expires in 6 days. Rotate it before "
                         "August 19."))
                    ->Banner()
                    ->WithSize(self->toolbar.size)
                    ->IntoEl());
    bcol->Child(component::Alert::Error(
                    cx, StrL("banner-error"),
                    StrL("Live updates are disconnected. Changes may be "
                         "delayed."))
                    ->Banner()
                    ->WithSize(self->toolbar.size)
                    ->IntoEl());
    StorySectionAdd(ban, AlertW(cx, bcol));
    page->Child(ban);

    El* custom =
        StorySection(cx, "Custom icon", "Custom icon and long content.");
    StorySectionAdd(
        custom,
        AlertW(cx, component::Alert::New(
                       cx, StrL("other-1"),
                       StrL("The quarterly planning review overlaps with the "
                            "APAC operations call. Move one event or invite "
                            "another owner before sending the agenda."))
                       ->Title(StrL("Two events overlap by 30 minutes"))
                       ->Icon(IconName::Calendar)
                       ->WithSize(self->toolbar.size)
                       ->IntoEl()));
    page->Child(custom);
    return page;
}

void AlertStory::Click(AlertStory* self, Ctx* cx, int id) {
    if (StoryToolbarClick(&self->toolbar, id)) {
        return;
    }
    (void)cx;
    if (id == ClickAlertBannerClose) {
        self->alertBanner = false;
    }
}

STORY_PAGE(StoryAlert, AlertStory);
