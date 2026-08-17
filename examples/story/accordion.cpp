#include "Story.h"

enum {
    ClickAccItem = 2100,
    ClickAccStyled = 2110,
    ClickAccMultiple = 2120,
    ClickAccIcon,
    ClickAccDisabled,
    ClickAccBordered,
};

static void ToggleOpen(bool* flags, int n, int i, bool multiple);
static void OnAccDefault(StoryApp* app, int i) {
    ToggleOpen(app->accordionOpen, 3, i, app->accordionMultiple);
}
static void OnAccStyled(StoryApp* app, int i) {
    ToggleOpen(app->accordionStyledOpen, 3, i, app->accordionMultiple);
}

static void ToggleOpen(bool* flags, int n, int i, bool multiple) {
    if (i < 0 || i >= n) {
        return;
    }
    if (multiple) {
        flags[i] = !flags[i];
        return;
    }
    bool next = !flags[i];
    for (int k = 0; k < n; k++) {
        flags[k] = false;
    }
    flags[i] = next;
}

static El* Opt(Arena* a, int id, const char* label, bool on) {
    const Theme& th = ThemeNow();
    El* box = Div(a)
                  ->W(16)
                  ->H(16)
                  ->Border(1, th.border)
                  ->Radius(3)
                  ->ItemsCenter()
                  ->JustifyCenter();
    if (on) {
        box->Bg(th.primary)->Child(StoryTxt(a, StrL("✓"), 11, th.primaryFg));
    }
    return Div(a)
        ->FlexRow()
        ->Gap(6)
        ->ItemsCenter()
        ->H(24)
        ->Click(id)
        ->Child(box)
        ->Child(StoryTxt(a, Str(label), 12, th.foreground));
}

El* AccordionRender(StoryApp* app, Arena* a) {
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill)->ItemsStart();
    page->Child(StoryToolbar(a, app));

    El* opts = Div(a)->FlexRow()->Gap(16)->ItemsCenter();
    opts->Child(Opt(a, ClickAccMultiple, "Multiple", app->accordionMultiple));
    opts->Child(Opt(a, ClickAccIcon, "Icons", app->accordionIcon));
    opts->Child(Opt(a, ClickAccDisabled, "Disabled", app->accordionDisabled));
    opts->Child(Opt(a, ClickAccBordered, "Bordered", app->accordionBordered));
    page->Child(opts);

    const char* titles[] = {"Is it accessible?", "Can it hold any content?", "Is it animated?"};
    const char* bodies[] = {
        "Yes. Each item is a button with an aria-expanded state, so screen readers announce whether the section is open, and the whole group can be reached with the keyboard.",
        "An item takes any element as its content, not just text. The height animation measures whatever you put in it.",
        "Yes. Expanding and collapsing animates the height of the content, and the chevron rotates to follow. Items below move along with it.",
    };

    component::Accordion* acc = component::Accordion::New(a, StrL("test"))
                                    ->Multiple(app->accordionMultiple)
                                    ->Bordered(app->accordionBordered)
                                    ->Disabled(app->accordionDisabled)
                                    ->OnToggle(MkFunc1(&OnAccDefault, app));
    for (int i = 0; i < 3; i++) {
        acc->Item(Str(titles[i]), Str(bodies[i]), app->accordionOpen[i]);
    }
    El* def = StorySection(a, "Default", "Expand one item at a time by default.");
    StorySectionAdd(def, Div(a)->W(480)->Child(acc->IntoEl()));
    page->Child(def);

    component::Accordion* styled = component::Accordion::New(a, StrL("custom-style"))
                                       ->Multiple(app->accordionMultiple)
                                       ->Bordered(false)
                                       ->Disabled(app->accordionDisabled)
                                       ->OnToggle(MkFunc1(&OnAccStyled, app));
    styled->Item(StrL("Account Settings"),
                 StrL("Manage your account preferences, security settings, and personal information."),
                 app->accordionStyledOpen[0]);
    styled->Item(StrL("Privacy & Security"), StrL("Control who can see your profile and how your data is used."),
                 app->accordionStyledOpen[1]);
    styled->Item(StrL("Help & Support"), StrL("Browse the documentation, or get in touch with the support team."),
                 app->accordionStyledOpen[2]);
    El* custom = StorySection(a, "Custom style", nullptr);
    El* frame = Div(a)
                    ->W(480)
                    ->Pad(4)
                    ->Radius(16)
                    ->Bg(RgbaOpacity(th.secondary, 0.5f))
                    ->Border(1, RgbaOpacity(th.border, 0.5f))
                    ->Child(styled->IntoEl());
    StorySectionAdd(custom, frame);
    page->Child(custom);
    (void)th;
    return page;
}

void AccordionClick(StoryApp* app, int id) {
    if (id == ClickAccMultiple) {
        app->accordionMultiple = !app->accordionMultiple;
        return;
    }
    if (id == ClickAccIcon) {
        app->accordionIcon = !app->accordionIcon;
        return;
    }
    if (id == ClickAccDisabled) {
        app->accordionDisabled = !app->accordionDisabled;
        return;
    }
    if (id == ClickAccBordered) {
        app->accordionBordered = !app->accordionBordered;
        return;
    }
    if (id >= ClickAccItem && id < ClickAccItem + 3) {
        ToggleOpen(app->accordionOpen, 3, id - ClickAccItem, app->accordionMultiple);
        return;
    }
    if (id >= ClickAccStyled && id < ClickAccStyled + 3) {
        ToggleOpen(app->accordionStyledOpen, 3, id - ClickAccStyled, app->accordionMultiple);
    }
}

STORY_PAGE(StoryAccordion, AccordionRender, AccordionClick);
