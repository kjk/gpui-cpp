/* Ported from crates/ui/src/menu/popup_menu.rs.
 *
 * Rust binds enter, escape, up, down, left and right in the "PopupMenu" key
 * context, and so does this — the element declares the context, the keymap
 * resolves the chord against it, and the menu reads the action. select_up and
 * select_down walk only the clickable rows — a separator and a label are
 * stepped over — and both ends wrap; the two odd cases are Rust's own:
 * select_down with nothing selected takes row 0 whether or not it is clickable,
 * and select_up with nothing selected takes the last clickable row. Left and
 * Right depend on the side the submenus open towards. */

#include "Test.h"

// The chord, through the keymap, to the thing the menu does about it —
// which is the whole path a keystroke takes now that the menu binds its keys
// instead of translating them.
static PopupMenuAction ForChord(const char* spec, Side side) {
    KeyChord c = {};
    utassert(KeyChordParse(Str(spec), &c));
    uint32_t ctx = KeyContextOf(PopupMenuContext());
    return PopupMenuActionOf(KeymapMatch(c, &ctx, 1).action, side);
}

static void TheBindingsAreTheOnesRustBinds() {
    PopupMenuInitKeys();

    utassert(ForChord("up", Side::Right) == PopupMenuAction::SelectPrev);
    utassert(ForChord("down", Side::Right) == PopupMenuAction::SelectNext);
    utassert(ForChord("enter", Side::Right) == PopupMenuAction::Confirm);
    utassert(ForChord("escape", Side::Right) == PopupMenuAction::Cancel);

    // A menu that opens to the right is reached into with Right.
    utassert(ForChord("right", Side::Right) == PopupMenuAction::OpenSubmenu);
    utassert(ForChord("left", Side::Right) == PopupMenuAction::CloseSubmenu);
    // One that opens to the left swaps them.
    utassert(ForChord("left", Side::Left) == PopupMenuAction::OpenSubmenu);
    utassert(ForChord("right", Side::Left) == PopupMenuAction::CloseSubmenu);

    // Nothing is bound to space in a menu, and an action from somewhere else
    // is not one of the menu's six.
    utassert(ForChord("space", Side::Right) == PopupMenuAction::None);
    utassert(PopupMenuActionOf(ActionOf(StrL("ui::SelectFirst")),
                               Side::Right) == PopupMenuAction::None);

    // Outside the menu's context the same chords resolve to nothing, which is
    // what keeps an arrow key from being the menu's while focus is elsewhere.
    KeyChord up = {};
    KeyChordParse(StrL("up"), &up);
    utassert(KeymapMatch(up, nullptr, 0).action == 0);
}

static void TheWalkStepsOverWhatCannotBeClicked() {
    // item, separator, item, label, item
    const bool clickable[5] = {true, false, true, false, true};

    utassert(PopupMenuNextIndex(clickable, 5, 0) == 2);
    utassert(PopupMenuNextIndex(clickable, 5, 2) == 4);
    // Past the last, back to the top.
    utassert(PopupMenuNextIndex(clickable, 5, 4) == 0);

    utassert(PopupMenuPrevIndex(clickable, 5, 4) == 2);
    utassert(PopupMenuPrevIndex(clickable, 5, 2) == 0);
    // Before the first, round to the last clickable one.
    utassert(PopupMenuPrevIndex(clickable, 5, 0) == 4);
}

static void NothingSelectedYet() {
    // Rust's select_down takes row 0 with nothing selected, even where row 0
    // is a separator.
    const bool clickable[3] = {false, true, true};
    utassert(PopupMenuNextIndex(clickable, 3, -1) == 0);
    // select_up takes the last clickable row instead.
    utassert(PopupMenuPrevIndex(clickable, 3, -1) == 2);
}

static void AMenuWithNoRows() {
    utassert(PopupMenuNextIndex(nullptr, 0, -1) == -1);
    utassert(PopupMenuPrevIndex(nullptr, 0, -1) == -1);
}

static void ALongStoryMenuFitsWithoutTruncation() {
    // There is no cap to fit inside any more; what this pins is that there
    // is not one. A hundred rows go in and a hundred come back out.
    PopupMenuState s;
    PopupMenuBeginRows(&s);
    for (int i = 0; i < 100; i++) {
        PopupMenuRow row;
        row.clickable = true;
        PopupMenuAddRow(&s, row);
    }
    utassert(s.rows.len == 100);
    PopupMenuBeginRows(&s);
    utassert(s.rows.len == 0);
}

// The whole path, as the element lays it out: a focusable root inside the
// "PopupMenu" context with the six actions on it, and a chord that walks in
// from the window. The themed element needs a paint backend to measure its
// rows, so this builds the same shape by hand.
static El* MenuLikeEl(Arena* a, Entity<PopupMenuState> menu, int focusId) {
    Listener onAction = ListenTo(menu, &PopupMenuState::OnAction);
    return Div(a)
        ->KeyContext(PopupMenuContext())
        ->FocusId(focusId)
        ->OnAction(action::Confirm(), onAction)
        ->OnAction(action::Cancel(), onAction)
        ->OnAction(action::SelectUp(), onAction)
        ->OnAction(action::SelectDown(), onAction);
}

static void AnOpenMenuAnswersTheChordItself() {
    KeymapClear();
    PopupMenuInitKeys();

    Arena* a = ArenaNew();
    App app;
    Window* win = new Window();
    win->app = &app;
    Entity<PopupMenuState> menu = EntityNewState<PopupMenuState>(&app);
    PopupMenuState* s = menu.Get(&app);
    s->open = true;
    // Three clickable rows, as the element would have recorded them.
    PopupMenuBeginRows(s);
    for (int i = 0; i < 3; i++) {
        PopupMenuRow row;
        row.clickable = true;
        PopupMenuAddRow(s, row);
    }

    El* root = Div(a)->Child(MenuLikeEl(a, menu, 7));
    FocusCollect(win, root);
    win->focusId = 7;

    // down, down: Rust's select_down takes row 0 first and then walks.
    utassert(WindowDispatchKeyAction(win, KeyDown, false, false, false));
    utassert(s->selected == 0);
    utassert(WindowDispatchKeyAction(win, KeyDown, false, false, false));
    utassert(s->selected == 1);
    utassert(WindowDispatchKeyAction(win, KeyUp, false, false, false));
    utassert(s->selected == 0);
    // escape closes it, and a menu that closes forgets its selection.
    utassert(WindowDispatchKeyAction(win, KeyEscape, false, false, false));
    utassert(!s->open && s->selected == -1);

    // Closed, the menu wants none of it: the action carries on outwards, the
    // way cx.propagate() does, and nothing else here keeps it.
    utassert(!WindowDispatchKeyAction(win, KeyDown, false, false, false));
    utassert(s->selected == -1);

    // And with focus outside the menu the chord is not even the menu's: the
    // context is not on the ancestry, so no binding matches at all.
    s->open = true;
    win->focusId = 0;
    utassert(!WindowDispatchKeyAction(win, KeyDown, false, false, false));
    utassert(s->selected == -1);

    delete win;
    ArenaDelete(a);
    KeymapClear();
}

static void TheMenuBarWrapsBothWays() {
    using namespace gpui::component;
    // on_move_right / on_move_left, over three titles.
    utassert(AppMenuBarNextIndex(0, 3) == 1);
    utassert(AppMenuBarNextIndex(2, 3) == 0);
    utassert(AppMenuBarPrevIndex(1, 3) == 0);
    utassert(AppMenuBarPrevIndex(0, 3) == 2);
    // Neither moves while nothing is open: Rust returns early on a None
    // selected_index.
    utassert(AppMenuBarNextIndex(-1, 3) == -1);
    utassert(AppMenuBarPrevIndex(-1, 3) == -1);
    // And an empty bar has nothing to move to.
    utassert(AppMenuBarNextIndex(0, 0) == 0);
}

void TestPopupMenu() {
    TheBindingsAreTheOnesRustBinds();
    TheWalkStepsOverWhatCannotBeClicked();
    NothingSelectedYet();
    AMenuWithNoRows();
    ALongStoryMenuFitsWithoutTruncation();
    AnOpenMenuAnswersTheChordItself();
    TheMenuBarWrapsBothWays();
}
