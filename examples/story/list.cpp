#include "Story.h"

// The Rust story fills its list with random companies; ours keeps a fixed
// set with the same shape, grouped by industry.
struct ListRow {
    const char* name;
    const char* price;
    const char* change;
    bool up;
};

struct ListSection {
    const char* industry;
    int section; // the index the Rust delegate prints in the header
    ListRow rows[4];
};

static const ListSection kSections[] = {
    {"Airlines / Aviation",
     1,
     {{"Daugherty and Sons", "422.23", "54.63%", true},
      {"Jaskolski and Rowe Inc", "958.26", "-14.56%", false},
      {"Windler and Sons", "329.63", "-12.71%", false},
      {"Jaskolski and Rowe Inc", "331.79", "21.49%", true}}},
    {"Automotive",
     4,
     {{"Walter Group", "137.70", "10.01%", true},
      {"Hilpert Group", "962.30", "-15.44%", false},
      {"Windler and Sons", "613.71", "15.03%", true},
      {"Hilpert Group", "48.07", "42.82%", true}}},
    {"Think Tanks",
     5,
     {{"Windler and Sons", "352.49", "131.67%", true},
      {"Hills LLC", "768.95", "-726.01%", false},
      {"Muller and Rippin Inc", "512.03", "-805.82%", false},
      {"Hills LLC", "477.77", "-26.30%", false}}},
};

enum {
    ListMenuGoTo = 1,
    ListMenuOptions
};

enum {
    ListActGoTop = 100,
    ListActGoSelected,
    ListActGoRow,
    ListActGoBottom,
    ListActSelectable,
    ListActSearchable,
    ListActLoading,
    ListActLazyLoad,
    ListActDraggable
};

struct ListStory {
    // ListState is an entity in Rust too, which is what the row closures and
    // cx.subscribe capture.
    Entity<ListState> list = {};
    int confirmedRow = -1;
    int openMenu = 0;
    bool selectable = true;
    bool searchable = true;
    bool loading = false;
    bool lazyLoad = false;
    bool draggable = false;
    InputState search;
    bool seeded = false;

    static El* Render(ListStory* self, Ctx* cx);
    static void OnKey(ListStory* self, Ctx* cx, const KeyEvent* ev);
};

// cx.subscribe(&list, ..): a Confirm is what the story acts on, and a Cancel
// clears what it was showing.
static void OnListEvent(ListStory* self, Ctx* cx, const ListEvent* ev) {
    if (ev->kind == ListEventKind::Confirm) {
        self->confirmedRow = ev->index;
    } else if (ev->kind == ListEventKind::Cancel) {
        self->confirmedRow = -1;
    }
    Notify(cx);
}

// The "List" key context: up and down walk the rows, Enter takes one, Escape
// gives up the selection. A page with the search field focused leaves the
// text keys to the field.
void ListStory::OnKey(ListStory* self, Ctx* cx, const KeyEvent* ev) {
    if (!ev->down) {
        return;
    }
    ListAction act = ListActionForKey(ev->vk);
    if (act == ListAction::None) {
        return;
    }
    ListState* st = self->list.Get(cx);
    if (st) {
        ListPerform(st, cx, act, ev->ctrl);
    }
}

static void ListMenuOpen(ListStory* self, Ctx* cx, const ClickEvent*,
                         intptr_t which) {
    self->openMenu = self->openMenu == (int)which ? 0 : (int)which;
    Notify(cx);
}
static void ListMenuAct(ListStory* self, Ctx* cx, const ClickEvent*,
                        intptr_t act) {
    switch (act) {
        case ListActSelectable:
            self->selectable = !self->selectable;
            break;
        case ListActSearchable:
            self->searchable = !self->searchable;
            break;
        case ListActLoading:
            self->loading = !self->loading;
            break;
        case ListActLazyLoad:
            self->lazyLoad = !self->lazyLoad;
            break;
        case ListActDraggable:
            self->draggable = !self->draggable;
            break;
        default:
            break;
    }
    self->openMenu = 0;
    Notify(cx);
}
static void FocusSearch(ListStory* self, Ctx* cx, const ClickEvent*) {
    self->search.focused = true;
    Notify(cx);
}

El* ListStory::Render(ListStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        InputSetPlaceholder(&self->search, StrL("Search..."));
        self->list = EntityNewState<ListState>(cx->app);
    }
    ListState* st = self->list.Get(cx);
    if (st) {
        st->selectable = self->selectable;
        st->onEvent = Listen(cx, &OnListEvent);
    }
    if (self->search.focused) {
        cx->win->input = &self->search;
    }
    Listener openMenu = Listen(cx, &ListMenuOpen);
    Listener act = Listen(cx, &ListMenuAct);

    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill);

    // story_toolbar_group() with two dropdowns: Go To and Options.
    El* toolbarRow = Div(a)->FlexRow()->W(kFill)->JustifyEnd()->ItemsStart();
    El* group = StoryToolbarGroup(cx);
    StoryToolbarOpt goTo[4] = {
        {"Top", false, ListActGoTop, true},
        {"Selected", false, ListActGoSelected, true},
        {"Section 5, Row 1", false, ListActGoRow, true},
        {"Bottom", false, ListActGoBottom, true},
    };
    group->Child(StoryToolbarDropdown(
        cx, StrL("list-go-to"), StrL("Go To"), self->openMenu == ListMenuGoTo,
        ListenerArg(openMenu, ListMenuGoTo), goTo, 4, act));
    group->Child(StoryToolbarDivider(cx));
    StoryToolbarOpt options[5] = {
        {"Selectable", self->selectable, ListActSelectable},
        {"Searchable", self->searchable, ListActSearchable},
        {"Loading", self->loading, ListActLoading},
        {"Lazy Load", self->lazyLoad, ListActLazyLoad},
        {"Draggable", self->draggable, ListActDraggable},
    };
    group->Child(StoryToolbarDropdown(cx, StrL("list-options"), StrL("Options"),
                                      self->openMenu == ListMenuOptions,
                                      ListenerArg(openMenu, ListMenuOptions),
                                      options, 5, act));
    toolbarRow->Child(group);
    page->Child(toolbarRow);

    // The list is the component now: it owns the rows' identity, the search
    // field, and what a click does to the selection.
    component::List* list =
        component::List::New(cx, StrL("list-story"), self->list)
            ->Loading(self->loading);
    if (self->searchable) {
        list->Searchable(&self->search, Listen(cx, &FocusSearch));
    }
    int rowIx = 0;
    for (size_t sec = 0; sec < sizeof(kSections) / sizeof(kSections[0]);
         sec++) {
        const ListSection& s = kSections[sec];
        El* head = Div(a)->FlexRow()->PadX(8)->PadB(4)->Gap(8)->ItemsCenter();
        head->Child(IconEl(a, IconName::Folder, 16)->Fg(th.mutedFg));
        head->Child(StoryTxt(cx, Str(s.industry), 14, th.mutedFg));
        head->Child(StoryTxt(cx, StoryFmt(cx, "(section: %d)", s.section), 14,
                             th.mutedFg));
        El* foot = Div(a)->PadX(8)->PadT(4)->PadB(20)->Child(
            StoryTxt(cx, StrL("Total 4 items in section."), 12, th.mutedFg));
        list->Section(head, foot);
        for (int i = 0; i < 4; i++) {
            const ListRow& r = s.rows[i];
            El* line = Div(a)
                           ->FlexRow()
                           ->W(kFill)
                           ->Gap(8)
                           ->ItemsCenter()
                           ->JustifyBetween();
            line->Child(StoryTxt(cx, Str(r.name), 16, th.foreground));
            El* right = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->JustifyEnd();
            right->Child(StoryTxt(cx, Str(r.price), 16, th.foreground)->W(65));
            right->Child(Div(a)->FlexRow()->W(65)->JustifyEnd()->Child(
                StoryTxt(cx, Str(r.change), 12, r.up ? th.green : th.red)
                    ->PadX(4)));
            line->Child(right);
            list->Item(component::ListItem::New(cx, line)
                           ->Confirmed(self->confirmedRow == rowIx));
            rowIx++;
        }
    }
    El* frame = list->IntoEl();
    page->Child(frame);
    return page;
}

STORY_PAGE_KEYS(StoryList, ListStory);
