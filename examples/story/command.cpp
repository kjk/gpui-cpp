#include "Story.h"

// crates/story/src/stories/command_story.rs: the palette in the shapes the
// component supports — grouped commands with a separator between them, a
// checked and a disabled item, keybinding hints, a list with no search field,
// custom rows of their own heights, and the empty and loading states.

struct CommandStory {
    Entity<component::CommandState> palette = {};
    Entity<component::CommandState> quick = {};
    Entity<component::CommandState> custom = {};
    Entity<component::CommandState> search = {};
    bool seeded = false;
    // What the last select and the last confirm were about, so the page shows
    // that the callbacks carry the item's place in the model as it was given
    // rather than where the filtering left it.
    Str selected = {};
    Str confirmed = {};

    static El* Render(CommandStory* self, Ctx* cx);
    static void OnSelect(CommandStory* self, Ctx* cx,
                         const component::CommandEvent* ev);
    static void OnConfirm(CommandStory* self, Ctx* cx,
                          const component::CommandEvent* ev);
    static void OnQuery(CommandStory* self, Ctx* cx,
                        const component::CommandEvent* ev);
};

static const Str kEmojiKeywords[] = {StrL("smile"), StrL("icon")};

static const component::CommandItem kSuggestions[] = {
    {StrL("Calendar"), nullptr, 0, IconName::Calendar},
    {StrL("Search Emoji"), kEmojiKeywords, 2, IconName::Search, 0, 0, nullptr,
     true},
    {StrL("Calculator"), nullptr, 0, IconName::Frame, 0, 0, nullptr, false,
     true},
};

// The second group carries shortcut hints: an item names an action, and the
// row shows whatever chord the keymap has bound to it.
static const component::CommandItem kSettings[] = {
    {StrL("Profile"), nullptr, 0, IconName::User, 0, 0, nullptr},
    {StrL("Billing"), nullptr, 0, IconName::CircleUser, 0, 0, nullptr},
    {StrL("Settings"), nullptr, 0, IconName::Settings, 0, 0, nullptr},
};

static const component::CommandItem kQuick[] = {
    {StrL("New File"), nullptr, 0, IconName::Plus},
    {StrL("Duplicate"), nullptr, 0, IconName::Copy},
    {StrL("Move to Trash"), nullptr, 0, IconName::X},
};

static const component::CommandItem kPopular[] = {
    {StrL("Apple"), nullptr, 0, IconName::Star},
    {StrL("Banana"), nullptr, 0, IconName::Star},
    {StrL("Cherry"), nullptr, 0, IconName::Star},
};

// Two custom rows of different heights. Rust measures every flattened row, so
// each keeps its own height inside the virtual list; a row here says how tall
// it is, which is the same thing said in the other direction.
static El* CompactRow(Ctx* cx, const component::CommandItem*) {
    return Div(cx->a)->FlexRow()->W(kFill)->Child(StoryTxt(
        cx, StrL("Compact custom row"), 14, ThemeNow(cx->app).foreground));
}
static El* ExpandedRow(Ctx* cx, const component::CommandItem*) {
    const Theme& th = ThemeNow(cx->app);
    return Div(cx->a)
        ->FlexCol()
        ->W(kFill)
        ->Gap(4)
        ->Child(StoryTxt(cx, StrL("Expanded custom row"), 14, th.foreground))
        ->Child(StoryTxt(
            cx, StrL("Its extra detail gives this row a different height."), 12,
            th.mutedFg));
}

static const component::CommandItem kCustomRows[] = {
    {StrL("small-row"), nullptr, 0, IconName::None, 0, 0, nullptr, false, false,
     CompactRow, 28},
    {StrL("large-row"), nullptr, 0, IconName::None, 0, 0, nullptr, false, false,
     ExpandedRow, 60},
};

static component::CommandEntry gSuggestionEntries[3] = {};
static component::CommandEntry gCustomEntries[2] = {};
static component::CommandEntry gPopularEntries[1] = {};

void CommandStory::OnSelect(CommandStory* self, Ctx* cx,
                            const component::CommandEvent* ev) {
    self->selected = StrDup(
        StoryFmt(cx, "section %d, row %d", ev->path.section, ev->path.row));
    Notify(cx);
}
void CommandStory::OnConfirm(CommandStory* self, Ctx* cx,
                             const component::CommandEvent* ev) {
    self->confirmed = StrDup(
        StoryFmt(cx, "section %d, row %d", ev->path.section, ev->path.row));
    Notify(cx);
}
void CommandStory::OnQuery(CommandStory* self, Ctx* cx,
                           const component::CommandEvent*) {
    // The asynchronous half of the API in its simplest form: a query with
    // nothing to answer it leaves the spinner off.
    component::CommandSetLoading(self->search.Get(cx), cx, false);
}

El* CommandStory::Render(CommandStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow(cx->app);
    if (!self->seeded) {
        self->seeded = true;
        self->palette = EntityNewState<component::CommandState>(cx->app);
        self->quick = EntityNewState<component::CommandState>(cx->app);
        self->custom = EntityNewState<component::CommandState>(cx->app);
        self->search = EntityNewState<component::CommandState>(cx->app);
        component::CommandGroup suggestions = {StrL("Suggestions"),
                                               kSuggestions, 3};
        component::CommandGroup settings = {StrL("Settings"), kSettings, 3};
        component::CommandGroup popular = {StrL("Popular"), kPopular, 3};
        gSuggestionEntries[0] = component::CommandEntryOf(suggestions);
        gSuggestionEntries[1] = component::CommandSeparatorEntry();
        gSuggestionEntries[2] = component::CommandEntryOf(settings);
        gCustomEntries[0] = component::CommandEntryOf(kCustomRows[0]);
        gCustomEntries[1] = component::CommandEntryOf(kCustomRows[1]);
        gPopularEntries[0] = component::CommandEntryOf(popular);
    }

    El* page = Div(a)->FlexCol()->W(kFill)->Gap(24);

    El* def = StorySection(cx, "Default",
                           "Two groups with a separator between them: a "
                           "checked item, a disabled one, and a search field "
                           "that filters both.");
    StorySectionAdd(
        def, component::Command::New(cx, StrL("command-default"), self->palette)
                 ->Entries(gSuggestionEntries, 3)
                 ->Placeholder(StrL("Type a command or search..."))
                 ->W(420)
                 ->OnSelect(Listen(cx, &CommandStory::OnSelect))
                 ->OnConfirm(Listen(cx, &CommandStory::OnConfirm))
                 ->IntoEl());
    StorySectionAdd(
        def, StoryTxt(cx,
                      StoryFmt(cx, "Selected: %s   Confirmed: %s",
                               self->selected.s ? self->selected.s : "—",
                               self->confirmed.s ? self->confirmed.s : "—"),
                      12, th.mutedFg));
    page->Child(def);

    El* quick =
        StorySection(cx, "Without a search field",
                     "`searchable(false)`: the arrows and Enter still work, "
                     "which is what a compact context menu wants.");
    StorySectionAdd(
        quick, component::Command::New(cx, StrL("command-quick"), self->quick)
                   ->Items(kQuick, 3)
                   ->Searchable(false)
                   ->W(420)
                   ->IntoEl());
    page->Child(quick);

    El* custom =
        StorySection(cx, "Custom rows",
                     "A row drawn by the caller keeps its own height inside "
                     "the virtual list.");
    StorySectionAdd(custom, component::Command::New(cx, StrL("command-custom"),
                                                    self->custom)
                                ->Entries(gCustomEntries, 2)
                                ->Placeholder(StrL("Filter rows..."))
                                ->W(420)
                                ->IntoEl());
    page->Child(custom);

    El* empty = StorySection(cx, "Empty",
                             "What a query with no match shows, and the "
                             "footer under the list.");
    StorySectionAdd(
        empty, component::Command::New(cx, StrL("command-empty"), self->search)
                   ->Entries(gPopularEntries, 1)
                   ->Placeholder(StrL("Search fruit..."))
                   ->W(420)
                   ->OnQuery(Listen(cx, &CommandStory::OnQuery))
                   ->Footer(Div(a)
                                ->W(kFill)
                                ->BorderT(1, th.border)
                                ->PadX(12)
                                ->PadY(8)
                                ->Child(StoryTxt(cx,
                                                 StrL("↑↓ to navigate · ↵ to "
                                                      "select · esc to clear"),
                                                 12, th.mutedFg)))
                   ->IntoEl());
    page->Child(empty);

    return page;
}

STORY_PAGE(StoryCommand, CommandStory);
