/* Ported from crates/ui/src/list/list.rs and list/cache.rs.
 *
 * Rust binds up, down, enter, secondary-enter and escape in the "List" key
 * context and hangs an on_action off each; this walks a chord in through the
 * keymap and pins what the list makes of it. The moves themselves
 * are rows_cache.next / .prev, which wrap at both ends and start from the
 * first or the last row when nothing is selected. The row walk here is over a
 * flat count rather than an IndexPath through sections, so the section
 * headers a Rust cache steps over have nothing to step over here. */

#include "Test.h"

// The chord, resolved in the list's context, read as what the list does.
static ListKeyAction ForChord(const char* spec) {
    ListInitKeys();
    KeyChord c = {};
    utassert(KeyChordParse(Str(spec), &c));
    uint32_t ctx = KeyContextOf(ListContext());
    KeyMatch m = KeymapMatch(c, &ctx, 1);
    return ListActionOf(m.action, m.arg);
}

static void TheKeyTable() {
    utassert(ForChord("up").action == ListAction::SelectPrev);
    utassert(ForChord("down").action == ListAction::SelectNext);
    utassert(ForChord("enter").action == ListAction::Confirm);
    utassert(ForChord("escape").action == ListAction::Cancel);
    utassert(ForChord("space").action == ListAction::None);
    utassert(ForChord("tab").action == ListAction::None);

    // `Confirm { secondary }` is bound twice: to enter and to
    // secondary-enter, and the two differ only in what the action carries.
    // The flag is the binding's `arg`, which the matcher hands back.
    utassert(!ForChord("enter").secondary);
    ListKeyAction sec = ForChord("secondary-enter");
    utassert(sec.action == ListAction::Confirm && sec.secondary);
    utassert(!ForChord("down").secondary);
}

static void NextAndPrevWrap() {
    ListState s;
    s.count = 3;

    // next(None) is the first row, prev(None) the last.
    utassert(ListNextIndex(&s) == 0);
    utassert(ListPrevIndex(&s) == 2);

    s.selected = 0;
    utassert(ListNextIndex(&s) == 1);
    utassert(ListPrevIndex(&s) == 2);

    s.selected = 2;
    utassert(ListNextIndex(&s) == 0);
    utassert(ListPrevIndex(&s) == 1);
}

static void AnEmptyListHasNowhereToGo() {
    ListState s;
    s.count = 0;
    utassert(ListNextIndex(&s) == -1);
    utassert(ListPrevIndex(&s) == -1);
}

static void TheFlattenedRowsAreHeaderItemsFooter() {
    ListState s;
    const int counts[] = {2, 3};
    ListSetSections(&s, counts, 2, true, true);
    // Two sections of two and three items, each with a header and a footer.
    utassert(s.count == 5);
    utassert(ListRowCount(&s) == 9);

    ListRow r = ListRowAt(&s, 0);
    utassert(r.kind == ListRowKind::SectionHeader && r.section == 0);
    r = ListRowAt(&s, 1);
    utassert(r.kind == ListRowKind::Entry && r.section == 0 && r.row == 0 &&
             r.entry == 0);
    r = ListRowAt(&s, 3);
    utassert(r.kind == ListRowKind::SectionFooter && r.section == 0);
    r = ListRowAt(&s, 4);
    utassert(r.kind == ListRowKind::SectionHeader && r.section == 1);
    r = ListRowAt(&s, 5);
    // The second section's first item carries on the item numbering, which is
    // what the selection is kept as.
    utassert(r.kind == ListRowKind::Entry && r.section == 1 && r.row == 0 &&
             r.entry == 2);
    r = ListRowAt(&s, 8);
    utassert(r.kind == ListRowKind::SectionFooter && r.section == 1);

    // And back the other way.
    utassert(ListRowOfEntry(&s, 0) == 1);
    utassert(ListRowOfEntry(&s, 2) == 5);
    utassert(ListRowOfEntry(&s, 4) == 7);
    utassert(ListRowOfEntry(&s, 5) == -1);
}

static void AnEmptySectionTakesItsHeaderWithIt() {
    ListState s;
    const int counts[] = {2, 0, 1};
    ListSetSections(&s, counts, 3, true, true);
    // The middle section contributes nothing at all — not even its header and
    // footer, which is what Rust's cache skips.
    utassert(s.count == 3);
    utassert(ListRowCount(&s) == 4 + 3);
    ListRow r = ListRowAt(&s, 4);
    utassert(r.kind == ListRowKind::SectionHeader && r.section == 2);
    utassert(ListRowOfEntry(&s, 2) == 5);
}

static void AListWithNoSectionsIsOneSection() {
    ListState s;
    ListSetCount(&s, 4);
    utassert(s.count == 4);
    // No header, no footer: a row is an item and nothing else.
    utassert(ListRowCount(&s) == 4);
    ListRow r = ListRowAt(&s, 2);
    utassert(r.kind == ListRowKind::Entry && r.entry == 2);
    utassert(ListRowOfEntry(&s, 3) == 3);
    // A row past the end is not an item.
    utassert(ListRowAt(&s, 9).entry == -1);
}

static void LoadMoreAsksNearTheEnd() {
    ListState s;
    ListSetCount(&s, 100);
    s.loadMoreThreshold = 20;
    // Nothing to load: the delegate said there is no more.
    utassert(!ListShouldLoadMore(&s, 95));
    s.hasMore = true;
    utassert(!ListShouldLoadMore(&s, 40));
    utassert(ListShouldLoadMore(&s, 80));
    utassert(ListShouldLoadMore(&s, 100));
    // A list already loading does not ask twice.
    s.loading = true;
    utassert(!ListShouldLoadMore(&s, 100));
}

void TestList() {
    TheKeyTable();
    NextAndPrevWrap();
    AnEmptyListHasNowhereToGo();
    TheFlattenedRowsAreHeaderItemsFooter();
    AnEmptySectionTakesItsHeaderWithIt();
    AListWithNoSectionsIsOneSection();
    LoadMoreAsksNearTheEnd();
}
