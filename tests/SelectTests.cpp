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

void TestSelect() {
    TestSuite("select");
    ArrowsOpenAClosedSelect();
    EnterOpensThenConfirms();
    EscapeOnlyCountsWhileOpen();
    ADisabledSelectAnswersToNothing();
    OtherKeysAreNotTheSelects();
    TheListInsideASelectIsTheContentHandle();
}
