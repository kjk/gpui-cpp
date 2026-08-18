#include "Story.h"

struct GroupBoxStory {
    bool email[3] = {false, false, false};
    bool profilePrivate = true;
    bool privateContrib = false;
    bool compactPrivate = false;
    int theme = 2;

    static El* Render(GroupBoxStory* self, Ctx* cx);
};

static void ToggleEmail(GroupBoxStory* self, Ctx* cx, const ClickEvent*,
                        intptr_t i) {
    if (i >= 0 && i < 3) {
        self->email[i] = !self->email[i];
    }
    Notify(cx);
}
static void TogglePrivate(GroupBoxStory* self, Ctx* cx, const ClickEvent*) {
    self->profilePrivate = !self->profilePrivate;
    Notify(cx);
}
static void ToggleContrib(GroupBoxStory* self, Ctx* cx, const ClickEvent*) {
    self->privateContrib = !self->privateContrib;
    Notify(cx);
}
static void ToggleCompact(GroupBoxStory* self, Ctx* cx, const ClickEvent*) {
    self->compactPrivate = !self->compactPrivate;
    Notify(cx);
}
static void PickTheme(GroupBoxStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t i) {
    self->theme = (int)i;
    Notify(cx);
}

// A row of text with a switch pushed to the far edge.
static El* SwitchRow(Ctx* cx, Str label, Str id, bool on, Listener onClick) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* row = Div(a)->FlexRow()->W(kFill)->ItemsCenter()->JustifyBetween();
    row->Child(StoryTxt(cx, label, 16, th.foreground));
    row->Child(component::Switch::New(cx, id)
                   ->Checked(on)
                   ->OnClick(onClick)
                   ->IntoEl());
    return row;
}

El* GroupBoxStory::Render(GroupBoxStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill)->ItemsCenter();

    // Default: the email options, with the primary action under them.
    El* def = StorySection(cx, "Default", nullptr);
    El* mail = Div(a)->FlexCol()->W(kFill)->Gap(8);
    static const char* kMail[3] = {"All activity", "Product updates",
                                   "Account activity"};
    static const char* kMailIds[3] = {"all", "news-letter", "account-activity"};
    for (int i = 0; i < 3; i++) {
        mail->Child(component::Checkbox::New(cx, Str(kMailIds[i]))
                        ->Label(Str(kMail[i]))
                        ->Checked(self->email[i])
                        ->OnClick(Listen(cx, &ToggleEmail, i))
                        ->IntoEl());
    }
    mail->Child(component::Button::New(cx, StrL("ok"))
                    ->Primary()
                    ->Label(StrL("Save preferences"))
                    ->IntoEl()
                    ->W(kFill));
    StorySectionAdd(
        def, Div(a)->W(512)->Child(
                 component::GroupBox::New(cx, StrL("Email notifications"))
                     ->Child(mail)
                     ->IntoEl()));
    page->Child(def);

    // Filled: two switch rows and a Save.
    El* filled = StorySection(cx, "Filled", nullptr);
    El* activity = Div(a)->FlexCol()->W(kFill)->Gap(8);
    activity
        ->Child(SwitchRow(cx, StrL("Make profile private and hide activity"),
                          StrL("profile-private"), self->profilePrivate,
                          Listen(cx, &TogglePrivate)));
    activity->Child(
        SwitchRow(cx, StrL("Include private contributions on my profile"),
                  StrL("private-contributions"), self->privateContrib,
                  Listen(cx, &ToggleContrib)));
    activity->Child(component::Button::New(cx, StrL("btn-1"))
                        ->Primary()
                        ->Label(StrL("Save"))
                        ->IntoEl()
                        ->W(kFill));
    StorySectionAdd(
        filled, Div(a)->W(512)->Child(component::GroupBox::New(
                                          cx, StrL("Contributions & activity"))
                                          ->Filled(true)
                                          ->Child(activity)
                                          ->IntoEl()));
    page->Child(filled);

    // Outlined: a vertical radio group.
    El* outlined = StorySection(cx, "Outlined", nullptr);
    El* themes = Div(a)->FlexCol()->W(kFill)->Gap(8);
    static const char* kThemes[3] = {"Light", "Dark", "System"};
    for (int i = 0; i < 3; i++) {
        themes->Child(component::Radio::New(cx, Str(kThemes[i]))
                          ->Label(Str(kThemes[i]))
                          ->Checked(self->theme == i)
                          ->OnClick(Listen(cx, &PickTheme, i))
                          ->IntoEl());
    }
    StorySectionAdd(outlined, Div(a)->W(512)->Child(component::GroupBox::New(
                                                        cx, StrL("Appearance"))
                                                        ->Outline()
                                                        ->Child(themes)
                                                        ->IntoEl()));
    page->Child(outlined);

    El* untitled = StorySection(cx, "Without Title", nullptr);
    StorySectionAdd(
        untitled,
        Div(a)->W(512)->Child(
            component::GroupBox::New(cx, Str{})
                ->Outline()
                ->Child(SwitchRow(
                    cx, StrL("Make profile private and hide activity"),
                    StrL("compact-private"), self->compactPrivate,
                    Listen(cx, &ToggleCompact)))
                ->IntoEl()));
    page->Child(untitled);
    return page;
}

STORY_PAGE(StoryGroupBox, GroupBoxStory);
