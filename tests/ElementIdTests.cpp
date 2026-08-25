/* GlobalElementId, folded — `IdsCollect` in src/gpui/gpui.cpp.
 *
 * GPUI identifies an element by the *stack* of ElementIds from the root down
 * to it (`Window::with_id` pushes and pops, `GlobalElementId` is the stack),
 * so a name only has to be unique among its siblings. A hit rect here is one
 * flat int, so the stack is folded into a hash of itself. These pin the four
 * things that follow from that: a name is scoped by its ancestors, an unnamed
 * element is transparent, the separator keeps two different paths apart, and
 * an explicit id still wins. */

#include "Test.h"

static El* Named(Arena* a, const char* name) {
    return Div(a)->PathId(Str(name));
}

// The case the whole thing exists for: two elements with the same name under
// different parents are two elements. This is what lets a widget be used
// twice on a page without its caller inventing a name the other will not
// also pick — `component::Table` naming every row `table-row-%d` is what it
// looks like when they cannot.
static void TheSameNameUnderTwoParentsIsTwoElements() {
    Arena* a = ArenaNew();
    El* root = Div(a);
    El* left = Div(a)->Id(StrL("left"));
    El* right = Div(a)->Id(StrL("right"));
    El* rowA = Named(a, "row-0");
    El* rowB = Named(a, "row-0");
    left->Child(rowA);
    right->Child(rowB);
    root->Child(left)->Child(right);

    IdsCollect(root);
    utassert(rowA->clickId != 0);
    utassert(rowB->clickId != 0);
    utassert(rowA->clickId != rowB->clickId);
    ArenaDelete(a);
}

// And the case that is still the caller's to avoid, exactly as in Rust: two
// siblings of one parent sharing a name are one element as far as the id is
// concerned.
static void TheSameNameUnderOneParentIsOneId() {
    Arena* a = ArenaNew();
    El* root = Div(a)->Id(StrL("box"));
    El* one = Named(a, "row-0");
    El* two = Named(a, "row-0");
    root->Child(one)->Child(two);

    IdsCollect(root);
    utassert(one->clickId == two->clickId);
    ArenaDelete(a);
}

// An element with no name of its own pushes nothing, which is what GPUI's
// `with_id` does for an element that declared no ElementId. A wrapper div
// between a widget and its row must not change the row's identity.
static void AnUnnamedElementIsTransparent() {
    Arena* a = ArenaNew();
    El* rootA = Div(a)->Id(StrL("group"));
    El* direct = Named(a, "row-0");
    rootA->Child(direct);

    El* rootB = Div(a)->Id(StrL("group"));
    El* wrapper = Div(a);
    El* wrapped = Named(a, "row-0");
    wrapper->Child(wrapped);
    rootB->Child(wrapper);

    IdsCollect(rootA);
    IdsCollect(rootB);
    utassert(direct->clickId == wrapped->clickId);
    ArenaDelete(a);
}

// The separator: "ab" then "c" is not "a" then "bc". Without one a fold is
// just a concatenation and two unrelated paths meet.
static void TheSeparatorKeepsTwoPathsApart() {
    Arena* a = ArenaNew();
    El* rootA = Div(a)->Id(StrL("ab"));
    El* childA = Named(a, "c");
    rootA->Child(childA);

    El* rootB = Div(a)->Id(StrL("a"));
    El* childB = Named(a, "bc");
    rootB->Child(childB);

    IdsCollect(rootA);
    IdsCollect(rootB);
    utassert(childA->clickId != childB->clickId);
    ArenaDelete(a);
}

// PathId asks for the focus id too; PathClick does not, which is the box that
// is a hit target and nothing else. An explicit FocusId — including
// FocusId(0), how a decorated wrapper stays out of the tab order — wins over
// both, and so does an explicit Click.
static void AnExplicitIdWins() {
    Arena* a = ArenaNew();
    El* root = Div(a)->Id(StrL("root"));
    El* pathBoth = Named(a, "a");
    El* pathClickOnly = Div(a)->PathClick(StrL("b"));
    El* cleared = Div(a)->PathId(StrL("c"))->FocusId(0);
    El* numbered = Div(a)->PathId(StrL("d"))->Click(4242);
    root->Child(pathBoth)
        ->Child(pathClickOnly)
        ->Child(cleared)
        ->Child(numbered);

    IdsCollect(root);
    utassert(pathBoth->clickId != 0);
    utassert(pathBoth->style.focusId == pathBoth->clickId);
    utassert(pathClickOnly->clickId != 0);
    utassert(pathClickOnly->style.focusId == 0);
    utassert(cleared->clickId != 0);
    utassert(cleared->style.focusId == 0);
    utassert(numbered->clickId == 4242);
    ArenaDelete(a);
}

// The same tree built twice gives the same ids, which is what lets focus
// survive a frame: the tree is thrown away and rebuilt, and `win->focusId` is
// the only thing that crosses.
static void TheSameTreeGivesTheSameIds() {
    Arena* a = ArenaNew();
    int first = 0;
    for (int pass = 0; pass < 2; pass++) {
        El* root = Div(a)->Id(StrL("page"));
        El* mid = Div(a)->Id(StrL("list"));
        El* leaf = Named(a, "row-7");
        mid->Child(leaf);
        root->Child(mid);
        IdsCollect(root);
        if (pass == 0) {
            first = leaf->clickId;
        } else {
            utassert(leaf->clickId == first);
        }
    }
    utassert(first != 0);
    ArenaDelete(a);
}

// FocusHandle — `cx.focus_handle()`. GPUI hands out a refcounted slotmap key
// that has nothing to do with the element's name; a handle here is an int from
// a counter, allocated below -1000 so it can never be mistaken for a hashed
// element id (positive) or the window chrome (-1..-4).
static void EveryHandleIsItsOwnAndOutOfTheHashedRange() {
    FocusHandle a = FocusHandleNew((App*)nullptr);
    FocusHandle b = FocusHandleNew((App*)nullptr);
    utassert(a.IsValid() && b.IsValid());
    utassert(a != b);
    utassert(a.id <= -1000 && b.id <= -1000);
    // A default handle is nothing, which is what an unset one means.
    FocusHandle none;
    utassert(!none.IsValid());
    utassert(none.id == 0);
}

// `div().track_focus(&handle)`: the box is focusable *as* the handle, and what
// it is hit as is a separate question. Until handles existed the two were
// forced to be one number, which is why a popover had to spell its focus id
// `HashClickId(id) * 31 + 1` to keep clear of its own click id.
static void TrackFocusIsIndependentOfTheHitId() {
    Arena* ar = ArenaNew();
    FocusHandle h = FocusHandleNew((App*)nullptr);
    El* root = Div(ar)->Id(StrL("root"));
    El* box = Div(ar)->PathId(StrL("box"))->TrackFocus(h);
    root->Child(box);

    IdsCollect(root);
    utassert(box->clickId > 0);
    utassert(box->style.focusId == h.id);
    utassert(box->style.focusId != box->clickId);
    ArenaDelete(ar);
}

void TestElementId() {
    TestSuite("element-id");
    TheSameNameUnderTwoParentsIsTwoElements();
    TheSameNameUnderOneParentIsOneId();
    AnUnnamedElementIsTransparent();
    TheSeparatorKeepsTwoPathsApart();
    AnExplicitIdWins();
    TheSameTreeGivesTheSameIds();
    EveryHandleIsItsOwnAndOutOfTheHashedRange();
    TrackFocusIsIndependentOfTheHitId();
}
