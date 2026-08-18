#include "Story.h"

struct AccordionStory {
    bool accordionOpen[3] = {true, false, false};
    bool accordionStyledOpen[3] = {true, false, false};
    StoryToolbarState toolbar;
    StoryAccordionOptions options;

    static El* Render(AccordionStory* self, Ctx* cx);
};

static void ToggleOpen(bool* flags, int n, int i, bool multiple);
static void OnAccDefault(AccordionStory* self, Ctx* cx, const ClickEvent*,
                         intptr_t i) {
    ToggleOpen(self->accordionOpen, 3, i, self->options.multiple);
}
static void OnAccStyled(AccordionStory* self, Ctx* cx, const ClickEvent*,
                        intptr_t i) {
    ToggleOpen(self->accordionStyledOpen, 3, i, self->options.multiple);
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

El* AccordionStory::Render(AccordionStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill)->ItemsStart();
    page->Child(StoryToolbarWithOptions(cx, self));

    const char* titles[] = {"Is it accessible?", "Can it hold any content?",
                            "Is it animated?"};
    const char* bodies[] = {
        "Yes. Each item is a button with an aria-expanded state, so screen "
        "readers announce whether the section is open, and the whole group can "
        "be reached with the keyboard.",
        "An item takes any element as its content, not just text. The height "
        "animation measures whatever you put in it.",
        "Yes. Expanding and collapsing animates the height of the content, and "
        "the chevron rotates to follow. Items below move along with it.",
    };

    component::Accordion* acc = component::Accordion::New(cx, StrL("test"))
                                    ->Multiple(self->options.multiple)
                                    ->Bordered(self->options.bordered)
                                    ->Disabled(self->options.disabled)
                                    ->WithSize(self->toolbar.size)
                                    ->OnToggle(Listen(cx, &OnAccDefault));
    for (int i = 0; i < 3; i++) {
        acc->Item(Str(titles[i]), Str(bodies[i]), self->accordionOpen[i]);
    }
    El* def =
        StorySection(cx, "Default", "Expand one item at a time by default.");
    StorySectionAdd(def, Div(a)->W(480)->Child(acc->IntoEl()));
    page->Child(def);

    component::Accordion* styled =
        component::Accordion::New(cx, StrL("custom-style"))
            ->Multiple(self->options.multiple)
            ->Bordered(true)
            ->Disabled(self->options.disabled)
            ->WithSize(self->toolbar.size)
            ->OnToggle(Listen(cx, &OnAccStyled));
    styled->SettingsItem(
        StrL("Account Settings"),
        StrL("Manage your account preferences, security settings, and "
             "personal information. You can also configure two-factor "
             "authentication here."),
        self->accordionStyledOpen[0], IconName::Settings, StrL("New"));
    styled->SettingsItem(
        StrL("Privacy & Security"),
        StrL("Control who can see your profile and how your data is used."),
        self->accordionStyledOpen[1], IconName::Eye, Str{});
    styled->SettingsItem(
        StrL("Help & Support"),
        StrL(
            "Browse the documentation, or get in touch with the support team."),
        self->accordionStyledOpen[2], IconName::Info, Str{});
    El* custom = StorySection(cx, "Custom style", nullptr);
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

STORY_PAGE(StoryAccordion, AccordionStory);
