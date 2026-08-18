#include "Story.h"

// The panels, in the order the sections use them. Rust keys them by name and
// starts SETTINGS, API_KEYS, COMPONENTS_DIR and PROFILE open.
enum {
    CollApiKeys = 8,
    CollComponentsDir,
    CollUiDir,
    CollCount
};

struct CollapsibleStory {
    bool collOpen[CollCount] = {false, false, false, true, true, true,
                                false, true,  true,  true, false};
    static El* Render(CollapsibleStory* self, Ctx* cx);
};

static void ToggleColl(CollapsibleStory* self, int i) {
    if (i >= 0 && i < CollCount) {
        self->collOpen[i] = !self->collOpen[i];
    }
}

static void OnColl(CollapsibleStory* self, Ctx* cx, const ClickEvent*,
                   intptr_t ix) {
    ToggleColl(self, (int)ix);
    Notify(cx);
}

static El* Chevron(Ctx* cx, bool open) {
    Arena* a = cx->a;
    return IconEl(a, open ? IconName::ChevronDown : IconName::ChevronRight, 14)
        ->Fg(cx->theme().mutedFg);
}

// A leaf of the tree: a file icon and its name, indented past the chevron
// the folder rows carry.
static El* FileRow(Ctx* cx, Str name) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    return Div(a)
        ->FlexRow()
        ->H(28)
        ->PadX(8)
        ->Gap(8)
        ->ItemsCenter()
        ->Radius(th.radius)
        ->HoverBg(th.accent)
        ->Child(Div(a)->W(12)->Shrink0())
        ->Child(IconEl(a, IconName::File, 12)->Fg(th.mutedFg))
        ->Child(StoryTxt(cx, name, 13, th.foreground));
}

// A branch: the chevron, an open or closed folder, and the name.
static El* FolderRow(CollapsibleStory* self, Ctx* cx, int key, Str name) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    bool open = self->collOpen[key];
    return Div(a)
        ->FlexRow()
        ->H(28)
        ->W(kFill)
        ->PadX(8)
        ->Gap(8)
        ->ItemsCenter()
        ->Radius(th.radius)
        ->HoverBg(th.accent)
        ->OnClick(Listen(cx, &OnColl, key))
        ->Child(Chevron(cx, open))
        ->Child(IconEl(a, open ? IconName::FolderOpen : IconName::Folder, 12)
                    ->Fg(th.mutedFg))
        ->Child(StoryTxt(cx, name, 13, th.foreground));
}

El* CollapsibleStory::Render(CollapsibleStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(8)->W(kFill);

    El* basic = StorySection(
        cx, "Basic",
        "A trigger beside the title, with a summary that stays visible.");
    El* orderHead =
        Div(a)->FlexRow()->W(360)->ItemsCenter()->JustifyBetween()->PadX(4);
    orderHead->Child(StoryTxt(cx, StrL("Order #4189"), 13, th.foreground)
                         ->Semibold());
    orderHead->Child(component::Button::New(cx, StrL("order-toggle"))
                         ->Ghost()
                         ->Icon(IconName::ChevronsUpDown)
                         ->Tooltip(StrL("Toggle details"))
                         ->IntoEl()
                         ->OnClick(Listen(cx, &OnColl, 0)));
    El* status = Div(a)
                     ->FlexRow()
                     ->W(360)
                     ->PadX(12)
                     ->PadY(8)
                     ->JustifyBetween()
                     ->ItemsCenter()
                     ->Border(1, th.border)
                     ->Radius(th.radius)
                     ->Bg(RgbaOpacity(th.muted, 0.3f));
    status->Child(StoryTxt(cx, StrL("Status"), 13, th.mutedFg));
    status->Child(component::Tag::New(cx, StrL("Shipped"))
                      ->Success()
                      ->WithSize(UiSize::Small)
                      ->IntoEl());
    El* orderBody = Div(a)->FlexCol()->Gap(8)->W(360);
    orderBody->Child(StoryTxt(cx, StrL("Shipping address"), 13, th.foreground)
                         ->Semibold());
    orderBody->Child(
        StoryTxt(cx, StrL("100 Market St, San Francisco"), 13, th.mutedFg));
    orderBody
        ->Child(StoryTxt(cx, StrL("Items"), 13, th.foreground)->Semibold());
    orderBody
        ->Child(StoryTxt(cx, StrL("2x Studio Headphones"), 13, th.mutedFg));
    StorySectionAdd(
        basic, component::Collapsible::New(cx)
                   ->Open(self->collOpen[0])
                   ->Trigger(Div(a)->FlexCol()->Gap(8)->Child(orderHead)->Child(
                       status))
                   ->Content(orderBody)
                   ->IntoEl());
    page->Child(basic);

    El* row =
        StorySection(cx, "Row trigger",
                     "The whole row is the trigger, as used by FAQ entries.");
    El* faqTrig = Div(a)
                      ->FlexRow()
                      ->W(360)
                      ->ItemsCenter()
                      ->JustifyBetween()
                      ->OnClick(Listen(cx, &OnColl, 1))
                      ->Child(StoryTxt(cx, StrL("How do I reset my password?"),
                                       13, th.foreground))
                      ->Child(Chevron(cx, self->collOpen[1]));
    El* faqBody = Div(a)->PadT(12)->W(360)->Child(
        StoryTxt(cx,
                 StrL("Click the Forgot Password link on the sign in page, "
                      "and we will send you an email with instructions to "
                      "create a new one."),
                 13, th.mutedFg)
            ->Wrap()
            ->MaxW(340));
    StorySectionAdd(row, component::Collapsible::New(cx)
                             ->Open(self->collOpen[1])
                             ->Trigger(faqTrig)
                             ->Content(faqBody)
                             ->IntoEl());
    page->Child(row);

    // Bottom trigger: a usage card whose chevron sits on its bottom edge.
    El* bottom = StorySection(
        cx, "Bottom trigger",
        "The trigger sits on the bottom edge of the card it opens.");
    El* usageHead =
        Div(a)->FlexRow()->W(kFill)->ItemsCenter()->JustifyBetween();
    usageHead->Child(
        StoryTxt(cx, StrL("3 days remaining in cycle"), 14, th.foreground));
    usageHead->Child(component::Button::New(cx, StrL("billing"))
                         ->Outline()
                         ->WithSize(UiSize::XSmall)
                         ->Label(StrL("Billing"))
                         ->IntoEl());
    El* usagePanel =
        Div(a)->FlexCol()->W(kFill)->Gap(8)->Pad(12)->Radius(th.radius)->Bg(
            RgbaOpacity(th.muted, 0.6f));
    El* usageTop = Div(a)->FlexRow()->W(kFill)->JustifyBetween();
    usageTop->Child(StoryTxt(cx, StrL("$18.08 / $20"), 14, th.foreground)
                        ->Semibold());
    usageTop->Child(StoryTxt(cx, StrL("$200"), 14, th.foreground)->Semibold());
    usagePanel->Child(usageTop);
    usagePanel
        ->Child(component::Progress::New(cx)->Value(90)->W(kFill)->IntoEl());

    static const char* kUsage[4][2] = {{"Requests", "$210.84"},
                                       {"Active CPU", "$21.95"},
                                       {"Events", "$21.20"},
                                       {"Storage", "$20.45"}};
    El* usageItems = Div(a)->FlexCol()->W(kFill)->Gap(8);
    for (int i = 0; i < 4; i++) {
        El* line = Div(a)->FlexRow()->W(kFill)->JustifyBetween();
        line->Child(StoryTxt(cx, Str(kUsage[i][0]), 12, th.mutedFg)
                        ->Semibold());
        line->Child(StoryTxt(cx, Str(kUsage[i][1]), 12, th.foreground)
                        ->Semibold());
        usageItems->Child(line);
    }

    El* usageBody = Div(a)->FlexCol()->W(kFill)->Gap(12)->Child(usagePanel);
    if (self->collOpen[2]) {
        usageBody->Child(usageItems);
    }
    El* card = Div(a)->FlexCol()->W(360)->Child(
        component::GroupBox::New(cx, Str{})
            ->Outline()
            ->Child(
                Div(a)->FlexCol()->W(kFill)->Gap(12)->Child(usageHead)->Child(
                    usageBody))
            ->IntoEl());
    // The toggle straddles the card's bottom border.
    card->Child(
        Div(a)
            ->Absolute()
            ->Bottom(-12)
            ->Left(0)
            ->W(kFill)
            ->FlexRow()
            ->JustifyCenter()
            ->Child(component::Button::New(cx, StrL("toggle-usage"))
                        ->Outline()
                        ->WithSize(UiSize::XSmall)
                        ->Icon(self->collOpen[2] ? IconName::ChevronUp
                                                 : IconName::ChevronDown)
                        ->Tooltip(StrL("Toggle details"))
                        ->IntoEl()
                        ->Radius(12)
                        ->Bg(th.background)
                        ->OnClick(Listen(cx, &OnColl, 2))));
    StorySectionAdd(bottom, card);
    page->Child(bottom);

    El* settings = StorySection(
        cx, "Settings",
        "Holds optional controls, keeping the default view short.");
    El* setTrig = component::Button::New(cx, StrL("settings"))
                      ->Outline()
                      ->Icon(self->collOpen[3] ? IconName::ChevronDown
                                               : IconName::ChevronRight)
                      ->Label(StrL("Notification settings"))
                      ->IntoEl()
                      ->W(360)
                      ->OnClick(Listen(cx, &OnColl, 3));
    El* setBody =
        Div(a)->FlexCol()->W(360)->Border(1, th.border)->Radius(th.radius);
    const char* notes[] = {"Push notifications", "Email notifications",
                           "SMS notifications"};
    for (int i = 0; i < 3; i++) {
        setBody->Child(Div(a)->PadX(12)->PadY(8)->Child(
            component::Checkbox::New(cx, StrDup(a, fmt("note-%d", i)))
                ->Label(Str(notes[i]))
                ->Checked(i == 0)
                ->IntoEl()));
    }
    StorySectionAdd(settings, component::Collapsible::New(cx)
                                  ->Open(self->collOpen[3])
                                  ->Trigger(setTrig)
                                  ->Content(setBody)
                                  ->IntoEl());
    page->Child(settings);

    // Row actions: buttons beside the trigger, in the header and every row.
    El* rowActions =
        StorySection(cx, "Row actions",
                     "Actions live beside the trigger, in the header and in "
                     "every row.");
    El* keysHead = Div(a)->FlexRow()->W(kFill)->Gap(8)->ItemsCenter();
    El* keysTrig = Div(a)->FlexRow()->Grow()->Gap(8)->ItemsCenter()->OnClick(
        Listen(cx, &OnColl, CollApiKeys));
    keysTrig->Child(Chevron(cx, self->collOpen[CollApiKeys]));
    keysTrig
        ->Child(StoryTxt(cx, StrL("API Keys"), 13, th.foreground)->Medium());
    keysHead->Child(keysTrig);
    keysHead->Child(component::Button::New(cx, StrL("add-key"))
                        ->Ghost()
                        ->WithSize(UiSize::XSmall)
                        ->Icon(IconName::Plus)
                        ->Tooltip(StrL("Add key"))
                        ->IntoEl());
    struct KeyRow {
        const char* name;
        const char* key;
    };
    static const KeyRow kKeys[] = {
        {"Production", "PRDK230454*242SDIFPPL"},
        {"Development", "DUILO30454*242SDIFUIP"},
        {"Staging", "IPPODAS230454*242SDI"},
    };
    El* keysBody = Div(a)->FlexCol()->Gap(8)->W(kFill);
    for (size_t i = 0; i < sizeof(kKeys) / sizeof(kKeys[0]); i++) {
        El* keyRow = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->W(kFill);
        keyRow->Child(
            Div(a)
                ->W(20)
                ->H(20)
                ->Shrink0()
                ->ItemsCenter()
                ->JustifyCenter()
                ->Radius(th.radius)
                ->Bg(th.muted)
                ->Child(IconEl(a, IconName::Asterisk, 12)->Fg(th.green)));
        keyRow->Child(StoryTxt(cx, Str(kKeys[i].name), 12, th.foreground)
                          ->W(80)
                          ->Shrink0());
        keyRow->Child(
            Div(a)
                ->Grow()
                ->PadX(8)
                ->PadY(2)
                ->Radius(th.radius)
                ->Bg(th.muted)
                ->Child(StoryTxt(cx, Str(kKeys[i].key), 12, th.foreground)));
        keyRow->Child(component::Button::New(cx, Str(kKeys[i].name))
                          ->Ghost()
                          ->WithSize(UiSize::XSmall)
                          ->Icon(IconName::Ellipsis)
                          ->Tooltip(StrL("More"))
                          ->IntoEl());
        keysBody->Child(keyRow);
    }
    StorySectionAdd(rowActions,
                    Div(a)->W(360)->Child(
                        component::GroupBox::New(cx, Str{})
                            ->Outline()
                            ->Child(component::Collapsible::New(cx)
                                        ->Open(self->collOpen[CollApiKeys])
                                        ->Trigger(keysHead)
                                        ->Content(keysBody)
                                        ->IntoEl())
                            ->IntoEl()));
    page->Child(rowActions);

    // Nested: panels inside panels, as a file tree.
    El* nested = StorySection(cx, "Nested",
                              "Panels nest to any depth, here as a file tree.");
    El* tree = Div(a)->FlexCol()->W(kFill);
    tree->Child(
        component::Collapsible::New(cx)
            ->Open(self->collOpen[CollComponentsDir])
            ->Trigger(
                FolderRow(self, cx, CollComponentsDir, StrL("components")))
            ->Content(
                Div(a)
                    ->FlexCol()
                    ->W(kFill)
                    ->PadL(12)
                    ->Child(component::Collapsible::New(cx)
                                ->Open(self->collOpen[CollUiDir])
                                ->Trigger(
                                    FolderRow(self, cx, CollUiDir, StrL("ui")))
                                ->Content(
                                    Div(a)
                                        ->FlexCol()
                                        ->W(kFill)
                                        ->PadL(12)
                                        ->Child(FileRow(cx, StrL("button.rs")))
                                        ->Child(FileRow(cx, StrL("card.rs")))
                                        ->Child(FileRow(cx, StrL("dialog.rs"))))
                                ->IntoEl())
                    ->Child(FileRow(cx, StrL("login_form.rs"))))
            ->IntoEl());
    tree->Child(FileRow(cx, StrL("main.rs")));
    StorySectionAdd(nested,
                    Div(a)->W(360)->Child(component::GroupBox::New(cx, Str{})
                                              ->Outline()
                                              ->Child(tree)
                                              ->IntoEl()));
    page->Child(nested);

    El* profile = StorySection(
        cx, "Profile",
        "Shows who someone is, and their details only on request.");
    El* profTrig =
        Div(a)->FlexRow()->W(360)->ItemsCenter()->JustifyBetween()->OnClick(
            Listen(cx, &OnColl, 7));
    El* who = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    who->Child(component::Avatar::New(cx)
                   ->Initials(StrL("JL"))
                   ->WithSize(UiSize::Small)
                   ->IntoEl());
    who->Child(StoryTxt(cx, StrL("@huacnlee"), 13, th.foreground)->Semibold());
    profTrig->Child(who)->Child(Chevron(cx, self->collOpen[7]));
    El* profBody = Div(a)->FlexCol()->Gap(8)->W(360);
    struct Field {
        IconName icon;
        const char* label;
        const char* value;
    };
    Field fields[] = {{IconName::Inbox, "Last activity", "2 hours ago"},
                      {IconName::Calendar, "Online since", "Today, 9:00 AM"},
                      {IconName::Globe, "Location", "Hong Kong"}};
    for (int i = 0; i < 3; i++) {
        El* f = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
        f->Child(IconEl(a, fields[i].icon, 12)->Fg(th.mutedFg));
        f->Child(StoryTxt(cx, Str(fields[i].label), 12, th.mutedFg));
        f->Child(StoryTxt(cx, Str(fields[i].value), 12, th.foreground)
                     ->Semibold());
        profBody->Child(f);
    }
    StorySectionAdd(profile, component::Collapsible::New(cx)
                                 ->Open(self->collOpen[7])
                                 ->Trigger(profTrig)
                                 ->Content(profBody)
                                 ->IntoEl());
    page->Child(profile);
    return page;
}

STORY_PAGE(StoryCollapsible, CollapsibleStory);
