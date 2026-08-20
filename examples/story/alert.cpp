#include "Story.h"

struct AlertStory {
    bool bannerVisible = true;
    StoryToolbarState toolbar;

    static El* Render(AlertStory* self, Ctx* cx);
};

static void ToggleBanner(AlertStory* self, Ctx* cx, const ClickEvent*) {
    self->bannerVisible = !self->bannerVisible;
    Notify(cx);
}
// println!("Info alert closed") — the close is wired, it just says so.
static void InfoClosed(AlertStory*, Ctx*, const ClickEvent*) {
    logf("Info alert closed\n");
}

// Every Alert section is .w_2_3(), which lands on the row inside the pane.
static El* AlertSection(Ctx* cx, const char* title, const char* desc) {
    El* sec = StorySection(cx, title, desc);
    StorySectionBody(sec)->WFrac(2.f / 3.f);
    return sec;
}

El* AlertStory::Render(AlertStory* self, Ctx* cx) {
    Arena* a = cx->a;
    UiSize size = self->toolbar.size;
    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* def =
        AlertSection(cx, "Default", "Title, icon, and rich text content.");
    StorySectionAdd(def,
                    (component::Alert::New(
                         cx, StrL("alert-default"),
                         StrL("Your workspace is ready for the team.\n"
                              "- **12 members** have access\n"
                              "- Billing remains with the workspace owner"))
                         ->Markdown()
                         ->WithSize(size)
                         ->Title(StrL("Workspace settings saved"))
                         ->IntoEl()));
    page->Child(def);

    El* vars = AlertSection(cx, "Variants",
                            "Info, success, warning, and error states.");
    El* col = Div(a)->FlexCol()->Gap(12)->W(kFill);
    col->Child(
        component::Alert::Info(cx, StrL("info1"),
                               StrL("Maintenance starts Friday at 22:00 UTC."))
            ->WithSize(size)
            ->Title(StrL("Scheduled maintenance"))
            ->OnClose(Listen(cx, &InfoClosed))
            ->IntoEl());
    col->Child(
        component::Alert::Success(cx, StrL("success-1"),
                                  StrL("The transfer is queued and usually "
                                       "settles within one business day."))
            ->WithSize(size)
            ->Title(StrL("Transfer submitted"))
            ->IntoEl());
    col->Child(
        component::Alert::Warning(
            cx, StrL("warning-1"),
            StrL("Two teammates still use recovery codes generated more than "
                 "a year ago.\n"
                 "Ask them to generate a fresh set in Security settings."))
            ->WithSize(size)
            ->IntoEl());
    col->Child(
        component::Alert::Error(
            cx, StrL("error-1"),
            StrL("Please verify your billing information and try again.\n"
                 "- Check your card details\n"
                 "- Ensure sufficient funds\n"
                 "- Verify billing address"))
            ->Markdown()
            ->WithSize(size)
            ->Title(StrL("Unable to process your payment."))
            ->IntoEl());
    StorySectionAdd(vars, (col));
    page->Child(vars);

    El* ban = AlertSection(cx, "Banner", "Full-width and closable alerts.");
    El* bcol = Div(a)->FlexCol()->Gap(8)->W(kFill);
    bcol->Child(
        component::Alert::New(
            cx, StrL("banner-1"),
            StrL("Reporting is read-only while the nightly ledger closes."))
            ->Banner()
            ->OnClose(Listen(cx, &ToggleBanner))
            ->Visible(self->bannerVisible)
            ->WithSize(size)
            ->IntoEl());
    bcol->Child(
        component::Alert::Info(
            cx, StrL("banner-info"),
            StrL("A new desktop update will install after you restart."))
            ->Banner()
            ->WithSize(size)
            ->IntoEl());
    bcol->Child(
        component::Alert::Success(cx, StrL("banner-success"),
                                  StrL("All 1,284 records finished importing."))
            ->Banner()
            ->WithSize(size)
            ->IntoEl());
    bcol->Child(component::Alert::Warning(
                    cx, StrL("banner-warning"),
                    StrL("Your API key expires in 6 days. Rotate it before "
                         "August 19."))
                    ->Banner()
                    ->WithSize(size)
                    ->IntoEl());
    bcol->Child(component::Alert::Error(
                    cx, StrL("banner-error"),
                    StrL("Live updates are disconnected. Changes may be "
                         "delayed."))
                    ->Banner()
                    ->WithSize(size)
                    ->IntoEl());
    StorySectionAdd(ban, (bcol));
    page->Child(ban);

    El* custom =
        AlertSection(cx, "Custom icon", "Custom icon and long content.");
    StorySectionAdd(custom,
                    (component::Alert::New(
                         cx, StrL("other-1"),
                         StrL("The quarterly planning review overlaps with the "
                              "APAC operations call. Move one event or invite "
                              "another owner before sending the agenda."))
                         ->Title(StrL("Two events overlap by 30 minutes"))
                         ->WithSize(size)
                         ->Icon(IconName::Calendar)
                         ->IntoEl()));
    page->Child(custom);
    return page;
}

STORY_PAGE(StoryAlert, AlertStory);
