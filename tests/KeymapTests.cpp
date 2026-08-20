/* Ported from crates/gpui's keymap: Keystroke::parse, KeyBinding, key
 * contexts, and the dispatch that reads them — which gpui-component builds
 * every component's keyboard on.
 *
 * An action is a name here rather than a type, so what is worth pinning is
 * the resolution: which binding a chord finds, which context wins, and how
 * far an action travels along the focused element's ancestry. */

#include "Test.h"

static void AChordIsReadTheWayRustSpellsIt() {
    KeyChord c = {};
    utassert(KeyChordParse(StrL("ctrl-c"), &c));
    utassert(c.vk == 'C' && c.ctrl && !c.shift && !c.alt);

    utassert(KeyChordParse(StrL("shift-tab"), &c));
    utassert(c.vk == KeyTab && c.shift && !c.ctrl);

    // cmd- and secondary- are the platform's shortcut key, which this port
    // folds onto ctrl everywhere — a Cmd-C handler and a Ctrl-C handler are
    // the same handler.
    utassert(KeyChordParse(StrL("cmd-c"), &c));
    utassert(c.vk == 'C' && c.ctrl);
    utassert(KeyChordParse(StrL("secondary-enter"), &c));
    utassert(c.vk == KeyReturn && c.ctrl);

    utassert(KeyChordParse(StrL("ctrl-shift-alt-i"), &c));
    utassert(c.vk == 'I' && c.ctrl && c.shift && c.alt);

    utassert(KeyChordParse(StrL("pagedown"), &c));
    utassert(c.vk == KeyPageDown && !c.ctrl);
    utassert(KeyChordParse(StrL("f12"), &c));
    utassert(c.vk == 123);

    // A dash is only a separator when something follows it, so the key can be
    // one: "alt-f4" and "ctrl--" both read.
    utassert(KeyChordParse(StrL("alt-f4"), &c));
    utassert(c.vk == 115 && c.alt);
    utassert(KeyChordParse(StrL("ctrl--"), &c));
    utassert(c.vk == 189 && c.ctrl);

    // Nonsense is a programming mistake, not something to dispatch.
    utassert(!KeyChordParse(StrL("hyper-c"), &c));
    utassert(!KeyChordParse(StrL("ctrl-nosuchkey"), &c));
    utassert(!KeyChordParse(Str{}, &c));
}

static KeyChord Chord(const char* spec) {
    KeyChord c = {};
    KeyChordParse(Str(spec), &c);
    return c;
}

// The context a binding is scoped to is tried before the ones outside it, and
// a binding with no context at all only after every scoped one has been.
static void TheInnermostContextWins() {
    KeymapClear();
    uint32_t cancel = ActionOf(StrL("menu::Cancel"));
    uint32_t close = ActionOf(StrL("dialog::Cancel"));
    uint32_t quit = ActionOf(StrL("app::Quit"));
    KeyBinding bindings[] = {
        {"escape", quit, nullptr},
        {"escape", close, "Dialog"},
        {"escape", cancel, "PopupMenu"},
    };
    KeymapBind(bindings, 3);

    uint32_t menu = KeyContextOf(StrL("PopupMenu"));
    uint32_t dialog = KeyContextOf(StrL("Dialog"));

    // A menu inside a dialog: the menu's binding is the one that answers.
    uint32_t stack[] = {menu, dialog};
    utassert(KeymapMatch(Chord("escape"), stack, 2) == cancel);
    // The dialog alone gets its own.
    utassert(KeymapMatch(Chord("escape"), stack + 1, 1) == close);
    // Neither, and the unscoped binding is what is left.
    utassert(KeymapMatch(Chord("escape"), nullptr, 0) == quit);
    // Nothing is bound to this one.
    utassert(KeymapMatch(Chord("ctrl-j"), stack, 2) == 0);
    KeymapClear();
}

// A later binding replaces an earlier one for the same chord and context,
// which is what a keymap layered over a default one has to do.
static void TheLastBindingForAChordWins() {
    KeymapClear();
    uint32_t first = ActionOf(StrL("a::First"));
    uint32_t second = ActionOf(StrL("a::Second"));
    KeyBinding a[] = {{"ctrl-k", first, nullptr}};
    KeyBinding b[] = {{"ctrl-k", second, nullptr}};
    KeymapBind(a, 1);
    KeymapBind(b, 1);
    utassert(KeymapMatch(Chord("ctrl-k"), nullptr, 0) == second);
    KeymapClear();
}

// Two names are two actions, the way two action types never compare equal.
static void AnActionIsItsName() {
    utassert(ActionOf(StrL("root::Tab")) == ActionOf(StrL("root::Tab")));
    utassert(ActionOf(StrL("root::Tab")) != ActionOf(StrL("root::TabPrev")));
    utassert(ActionOf(Str{}) != 0);
}

// --- dispatch -------------------------------------------------------------

static int gCalls = 0;
static uint32_t gSeen[8];

// A listener needs a live view to call into, the way GPUI's does; the test
// makes one so the dispatch is exercised end to end rather than stopping at
// a handle that points at nothing.
struct Recorder {
    static El* Render(Recorder*, Ctx* cx) { return Div(cx->a); }

    static void Stop(Recorder*, Ctx*, const ActionEvent* ev) {
        gSeen[gCalls++ & 7] = ev->action;
    }
    static void Pass(Recorder*, Ctx*, const ActionEvent* ev) {
        gSeen[gCalls++ & 7] = ev->action;
        // cx.propagate(): looked, did not want it.
        const_cast<ActionEvent*>(ev)->propagate = true;
    }
};

// The action goes to the focused element's own handler first and then out
// through its ancestors, and a handler that does not propagate ends it.
static void AnActionWalksOutFromWhatHasFocus() {
    KeymapClear();
    uint32_t act = ActionOf(StrL("t::Go"));
    KeyBinding b[] = {{"ctrl-g", act, "Inner"}};
    KeymapBind(b, 1);

    Arena* a = ArenaNew();
    App app;
    Window* win = new Window();
    win->app = &app;
    Entity<Recorder> rec = EntityNew<Recorder>(&app);
    El* leaf = Div(a)->FocusId(5)->OnAction(act, ListenTo(rec, &Recorder::Stop));
    El* mid = Div(a)->KeyContext(StrL("Inner"))->Child(leaf);
    El* root = Div(a)->OnAction(act, ListenTo(rec, &Recorder::Stop))->Child(mid);
    FocusCollect(win, root);
    win->focusId = 5;

    gCalls = 0;
    utassert(WindowDispatchKeyAction(win, 'G', false, true, false));
    // Only the leaf's: it kept the action.
    utassert(gCalls == 1);

    delete win;
    ArenaDelete(a);
    KeymapClear();
}

// A handler that propagates hands the action on outwards, and the outer one
// is what ends it.
static void PropagateCarriesOnOutwards() {
    KeymapClear();
    uint32_t act = ActionOf(StrL("t::Go2"));
    KeyBinding b[] = {{"ctrl-g", act, nullptr}};
    KeymapBind(b, 1);

    Arena* a = ArenaNew();
    App app;
    Window* win = new Window();
    win->app = &app;
    Entity<Recorder> rec = EntityNew<Recorder>(&app);
    El* leaf = Div(a)->FocusId(5)->OnAction(act, ListenTo(rec, &Recorder::Pass));
    El* root = Div(a)->OnAction(act, ListenTo(rec, &Recorder::Stop))->Child(leaf);
    FocusCollect(win, root);
    win->focusId = 5;

    gCalls = 0;
    utassert(WindowDispatchKeyAction(win, 'G', false, true, false));
    utassert(gCalls == 2);
    utassert(gSeen[0] == act && gSeen[1] == act);

    delete win;
    ArenaDelete(a);
    KeymapClear();
}

// A binding scoped to a context nothing along the focused ancestry declares
// does not match, however deeply the element is nested.
static void AContextOffThePathDoesNotMatch() {
    KeymapClear();
    uint32_t act = ActionOf(StrL("t::Go3"));
    KeyBinding b[] = {{"ctrl-g", act, "Elsewhere"}};
    KeymapBind(b, 1);

    Arena* a = ArenaNew();
    App app;
    Window* win = new Window();
    win->app = &app;
    Entity<Recorder> rec = EntityNew<Recorder>(&app);
    // The context is on a sibling, not on an ancestor.
    El* leaf = Div(a)->FocusId(5)->OnAction(act, ListenTo(rec, &Recorder::Stop));
    El* root = Div(a)
                   ->Child(Div(a)->KeyContext(StrL("Elsewhere")))
                   ->Child(Div(a)->Child(leaf));
    FocusCollect(win, root);
    win->focusId = 5;

    gCalls = 0;
    utassert(!WindowDispatchKeyAction(win, 'G', false, true, false));
    utassert(gCalls == 0);

    delete win;
    ArenaDelete(a);
    KeymapClear();
}

// The same, for an element that declares neither a context nor a handler: it
// still has to sit inside its own subtree rather than at the end of the one
// beside it, or it inherits the context of a sibling that closed there.
static void AFocusableWithNothingOfItsOwnStaysOnItsOwnPath() {
    KeymapClear();
    uint32_t act = ActionOf(StrL("t::Go4"));
    KeyBinding b[] = {{"ctrl-g", act, "Elsewhere"}};
    KeymapBind(b, 1);

    Arena* a = ArenaNew();
    App app;
    Window* win = new Window();
    win->app = &app;
    Entity<Recorder> rec = EntityNew<Recorder>(&app);
    El* root = Div(a)
                   ->Child(Div(a)->KeyContext(StrL("Elsewhere")))
                   ->Child(Div(a)->FocusId(5))
                   ->OnAction(act, ListenTo(rec, &Recorder::Stop));
    FocusCollect(win, root);
    win->focusId = 5;

    gCalls = 0;
    utassert(!WindowDispatchKeyAction(win, 'G', false, true, false));
    utassert(gCalls == 0);

    delete win;
    ArenaDelete(a);
    KeymapClear();
}

void TestKeymap() {
    TestSuite("keymap");
    AChordIsReadTheWayRustSpellsIt();
    TheInnermostContextWins();
    TheLastBindingForAChordWins();
    AnActionIsItsName();
    AnActionWalksOutFromWhatHasFocus();
    PropagateCarriesOnOutwards();
    AContextOffThePathDoesNotMatch();
    AFocusableWithNothingOfItsOwnStaysOnItsOwnPath();
}
