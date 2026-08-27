/* Ported from crates/base/src/select.rs.
 *
 * Rust binds up, down, enter, secondary-enter and escape in the select's key
 * context and hangs an on_action off each. Every one of those handlers is a
 * few lines of rules over `open` and `disabled`; this walks the chord in
 * through the keymap and pins what comes out. The focus transfer each handler
 * also does is the pair of handles the state keeps — the trigger's and the
 * list's — and the last case here pins that the second of them names a real
 * element, since a handle that names nothing restores focus by coincidence
 * rather than by containment. */

#include "Test.h"

// The chord, resolved in the select's context, read as what the select does.
static SelectAction ForChord(const char* spec, bool open, bool disabled) {
    SelectInitKeys();
    KeyChord c = {};
    utassert(KeyChordParse(Str(spec), &c));
    uint32_t ctx = KeyContextOf(SelectContext());
    return SelectActionOf(KeymapMatch(c, &ctx, 1).action, open, disabled);
}

static void ArrowsOpenAClosedSelect() {
    utassert(ForChord("down", false, false) == SelectAction::Open);
    utassert(ForChord("up", false, false) == SelectAction::Open);
    // Once open the root is done with them: Rust has focused the content by
    // then, so the list takes the arrow.
    utassert(ForChord("down", true, false) == SelectAction::None);
    utassert(ForChord("up", true, false) == SelectAction::None);
}

static void EnterOpensThenConfirms() {
    // secondary-enter is Confirm { secondary: true } in Rust, which has no
    // payload to carry here — it is its own name and the same answer.
    utassert(ForChord("secondary-enter", true, false) == SelectAction::Confirm);
    utassert(ForChord("enter", false, false) == SelectAction::Open);
    utassert(ForChord("enter", true, false) == SelectAction::Confirm);
}

static void EscapeOnlyCountsWhileOpen() {
    utassert(ForChord("escape", true, false) == SelectAction::Dismiss);
    // Closed, Rust propagates it so whatever encloses the select can use it.
    utassert(ForChord("escape", false, false) == SelectAction::None);
}

static void ADisabledSelectAnswersToNothing() {
    utassert(ForChord("down", false, true) == SelectAction::None);
    utassert(ForChord("up", true, true) == SelectAction::None);
    utassert(ForChord("enter", true, true) == SelectAction::None);
    utassert(ForChord("escape", true, true) == SelectAction::None);
}

static void OtherKeysAreNotTheSelects() {
    utassert(ForChord("tab", true, false) == SelectAction::None);
    utassert(ForChord("space", true, false) == SelectAction::None);
    utassert(ForChord("backspace", false, false) == SelectAction::None);
}

// `content_focus_handle` is `state.list.focus_handle(cx)` upstream, tracked
// on the list's own element. Ours is on the shared state, and the list inside
// a select tracks it: the focus a select moves into its dropdown has to land
// on something the frame can name, or a query field taking focus inside the
// list stops reading as the list's and closing leaves focus on an input that
// has gone. Not a tab stop — upstream asks for `.tab_stop(true)` on the
// trigger and nowhere else, so Tab walks past an open dropdown.
static void TheListInsideASelectIsTheContentHandle() {
    App app;
    Window* win = new Window();
    win->app = &app;
    Arena* a = ArenaNew();
    Ctx cx = {};
    cx.app = &app;
    cx.win = win;
    cx.a = a;

    using namespace gpui::component;
    Entity<SearchableListState> state =
        EntityNewState<SearchableListState>(&app);
    SearchableListState* s = state.Get(&app);
    s->contentFocus = FocusHandleNew(&cx);

    SearchableItem items[2] = {};
    items[0].title = StrL("Rust");
    items[0].value = StrL("rust");
    items[1].title = StrL("Go");
    items[1].value = StrL("go");
    SearchableListSearch(s, items, 2, Str{});

    El* box = SearchableList::New(&cx, StrL("list"), state, nullptr)
                  ->InSelect(true)
                  ->Items(items, 2)
                  ->IntoEl();
    utassert(box->style.focusId == s->contentFocus.id);
    utassert(!box->style.tabStop);

    // A list that is not inside a select is its own thing: it keeps the focus
    // id off its name and stays in the tab order, since it is what the reader
    // tabs to.
    El* alone = SearchableList::New(&cx, StrL("list"), state, nullptr)
                    ->Items(items, 2)
                    ->IntoEl();
    utassert(alone->style.focusId == HashClickId(StrL("list")));
    utassert(alone->style.tabStop);

    ArenaDelete(a);
    delete win;
    EntityDropAll(&app);
}

static void CaretKeepsTheSourceSizeScale() {
    using namespace gpui::component;
    utassertnear(Caret::New(UiSize::XSmall).IconSize(), 12.f);
    utassertnear(Caret::New(UiSize::Small).IconSize(), 14.f);
    utassertnear(Caret::New(UiSize::Medium).IconSize(), 16.f);
    utassertnear(Caret::New(UiSize::Large).IconSize(), 16.f);

    Arena* a = ArenaNew();
    Rgba color = Rgba{10, 20, 30, 255};
    El* icon = Caret::New(UiSize::Small).TextColor(color).IntoEl(a);
    utassert(StrSame(icon->iconPath, StrL("icons/chevron-down.svg")));
    utassertnear(icon->style.width, 14.f);
    utassert(icon->style.hasColor);
    utassert(icon->style.color.g == 20);
    ArenaDelete(a);
}

struct SelectEventSink {
    int count = 0;
    component::SelectEvent last = {};

    static void OnConfirm(SelectEventSink* self, Ctx*,
                          const component::SelectEvent* event) {
        self->count++;
        self->last = *event;
    }
};

static void SelectStateOwnsCommittedSelectionAndEvents() {
    using namespace gpui::component;
    App app;
    Window* win = new Window();
    win->app = &app;
    Ctx cx = {};
    cx.app = &app;
    cx.win = win;

    Entity<SelectState> state = SelectState::New(&app);
    SelectState* s = state.Get(&app);
    utassert(s != nullptr);
    Entity<SearchableListState> list = SelectListEntity(state);
    utassert(list.Get(&app) == s->List());

    SearchableItem items[] = {
        {StrL("Rust"), StrL("rust"), 0},
        {StrL("C++"), StrL("cpp"), 0},
        {StrL("Swift"), StrL("swift"), 2},
    };
    s->SetItems(items, 3);
    IndexPath swift = IndexPathNew(0).Section(2);
    s->SetSelectedIndex(&swift, &cx);
    IndexPath selected;
    utassert(s->SelectedIndex(&selected));
    utassert(selected == swift);
    utassert(StrSame(s->SelectedValue(), StrL("swift")));

    s->SetSelectedValue(StrL("cpp"), &cx);
    utassert(s->SelectedIndex(&selected));
    utassert(selected == IndexPathNew(1));

    s->Searchable(true);
    InputSetValue(&s->queryInput, StrL("Swift"));
    SearchableListSearch(s->List(), items, 3, InputValue(&s->queryInput));
    utassert(s->state.matches.len == 1);
    s->SetSelectedValue(StrL("rust"), &cx);
    utassert(InputValue(&s->queryInput).len == 0);
    utassert(s->state.matches.len == 3);

    Entity<SelectEventSink> sink = EntityNewState<SelectEventSink>(&app);
    SubscribeTo(&app, state, sink, &SelectEventSink::OnConfirm);
    ListEvent confirm = {ListEventKind::Confirm, 1, false};
    SelectState::OnListChange(s, &cx, &confirm);
    SelectEventSink* heard = sink.Get(&app);
    utassert(heard->count == 1);
    utassert(heard->last.hasValue);
    utassert(StrSame(heard->last.value, StrL("cpp")));

    s->Clean(&cx);
    utassert(!s->SelectedIndex(nullptr));
    utassert(heard->count == 2);
    utassert(!heard->last.hasValue);

    delete win;
    EntityDropAll(&app);
}

static void SourceSelectBuilderWritesItsOwnState() {
    using namespace gpui::component;
    App app;
    Window* win = new Window();
    win->app = &app;
    Arena* a = ArenaNew();
    Ctx cx = {};
    cx.app = &app;
    cx.win = win;
    cx.a = a;

    Entity<SelectState> state = SelectState::New(&app);
    SearchableItem items[] = {{StrL("One"), StrL("one")}};
    component::Select::New(&cx, StrL("source-select"), state)
        ->Items(items, 1)
        ->Icon(IconName::Search)
        ->TitlePrefix(StrL("Value: "))
        ->FocusRing(false)
        ->IntoEl();
    SelectState* s = state.Get(&app);
    utassert(s->state.items == items);
    utassert(s->state.nItems == 1);
    utassert(s->icon == IconName::Search);
    utassert(StrSame(s->titlePrefix, StrL("Value: ")));
    utassert(!s->focusRingEnabled);

    ArenaDelete(a);
    delete win;
    EntityDropAll(&app);
}

void TestSelect() {
    TestSuite("select");
    ArrowsOpenAClosedSelect();
    EnterOpensThenConfirms();
    EscapeOnlyCountsWhileOpen();
    ADisabledSelectAnswersToNothing();
    OtherKeysAreNotTheSelects();
    TheListInsideASelectIsTheContentHandle();
    CaretKeepsTheSourceSizeScale();
    SelectStateOwnsCommittedSelectionAndEvents();
    SourceSelectBuilderWritesItsOwnState();
}
