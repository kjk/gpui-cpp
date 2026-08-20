/* Ported from crates/ui/src/menu/popup_menu.rs.
 *
 * Rust binds enter, escape, up, down, left and right in the "PopupMenu" key
 * context. select_up and select_down walk only the clickable rows — a
 * separator and a label are stepped over — and both ends wrap; the two odd
 * cases are Rust's own: select_down with nothing selected takes row 0 whether
 * or not it is clickable, and select_up with nothing selected takes the last
 * clickable row. Left and Right depend on the side the submenus open
 * towards. */

#include "Test.h"

static void TheKeyTableDependsOnTheSide() {
    utassert(PopupMenuActionForKey(KeyUp, Side::Right) ==
             PopupMenuAction::SelectPrev);
    utassert(PopupMenuActionForKey(KeyDown, Side::Right) ==
             PopupMenuAction::SelectNext);
    utassert(PopupMenuActionForKey(KeyReturn, Side::Right) ==
             PopupMenuAction::Confirm);
    utassert(PopupMenuActionForKey(KeyEscape, Side::Right) ==
             PopupMenuAction::Cancel);

    // A menu that opens to the right is reached into with Right.
    utassert(PopupMenuActionForKey(KeyRight, Side::Right) ==
             PopupMenuAction::OpenSubmenu);
    utassert(PopupMenuActionForKey(KeyLeft, Side::Right) ==
             PopupMenuAction::CloseSubmenu);
    // One that opens to the left swaps them.
    utassert(PopupMenuActionForKey(KeyLeft, Side::Left) ==
             PopupMenuAction::OpenSubmenu);
    utassert(PopupMenuActionForKey(KeyRight, Side::Left) ==
             PopupMenuAction::CloseSubmenu);

    utassert(PopupMenuActionForKey(KeySpace, Side::Right) ==
             PopupMenuAction::None);
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
    // Label + 100 items + a separator before every group of five.
    utassert(gpui::component::kPopupMenuMaxItems >= 1 + 100 + 20);
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
    TheKeyTableDependsOnTheSide();
    TheWalkStepsOverWhatCannotBeClicked();
    NothingSelectedYet();
    AMenuWithNoRows();
    ALongStoryMenuFitsWithoutTruncation();
    TheMenuBarWrapsBothWays();
}
