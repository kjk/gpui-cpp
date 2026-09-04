/* crates/base/src/index_path.rs tests, plus the two conversions this tree
   needs because its lists key on the flat entry index instead. */

#include "Test.h"

// test_index_path
static void TheBuildersSetOnePartEach() {
    IndexPath p;
    utassert(p.section == 0 && p.row == 0 && p.column == 0);

    p = p.Section(1).Row(2).Column(3);
    utassert(p.section == 1 && p.row == 2 && p.column == 3);

    // IndexPath::new(row): the row, with the other two left at zero.
    IndexPath n = IndexPathNew(2);
    utassert(n.section == 0 && n.row == 2 && n.column == 0);

    // eq_row ignores the column; equality does not.
    utassert(p.EqRow(IndexPathNew(2).Section(1).Column(9)));
    utassert(p != IndexPathNew(2).Section(1).Column(9));
    utassert(!p.EqRow(IndexPathNew(2).Section(0)));
    utassert(p == IndexPathNew(2).Section(1).Column(3));
}

// test_into_element_id
static void ThePathNamesItsElement() {
    Arena* a = ArenaNew();
    IndexPath p = IndexPathNew(2).Section(1).Column(3);
    Str id = IndexPathIdStr(a, p);
    utassert(base::StrEq(id, StrL("index-path(1,2,3)")));
    // The hashed form is what El::Click takes, and two different paths do not
    // land on the same id.
    utassert(IndexPathClickId(p) ==
             (uint32_t)HashClickId(StrL("index-path(1,2,3)")));
    utassert(IndexPathClickId(p) != IndexPathClickId(IndexPathNew(3)));
    ArenaDelete(a);
}

// The conversions: an entry index and an IndexPath are the same place.
static void AnEntryAndAPathAreTheSamePlace() {
    ListState s;
    int counts[3] = {2, 0, 3};
    ListSetSections(&s, counts, 3, true, false);
    utassert(s.count == 5);

    // Section 1 is empty, so its entries are section 2's.
    utassert(ListIndexPathOf(&s, 0) == IndexPathNew(0).Section(0));
    utassert(ListIndexPathOf(&s, 1) == IndexPathNew(1).Section(0));
    utassert(ListIndexPathOf(&s, 2) == IndexPathNew(0).Section(2));
    utassert(ListIndexPathOf(&s, 4) == IndexPathNew(2).Section(2));

    for (int i = 0; i < s.count; i++) {
        utassert(ListEntryOf(&s, ListIndexPathOf(&s, i)) == i);
    }

    // Off either end answers rather than reading past the array.
    utassert(ListIndexPathOf(&s, 5).row == -1);
    utassert(ListIndexPathOf(&s, -1).row == -1);
    utassert(ListEntryOf(&s, IndexPathNew(0).Section(1)) == -1);
    utassert(ListEntryOf(&s, IndexPathNew(3).Section(2)) == -1);
    utassert(ListEntryOf(&s, IndexPathNew(-1)) == -1);

    // And the flattened row at a position carries the same path, so a row
    // element and a selection agree about where they are.
    int rowIx = ListRowOfEntry(&s, 4);
    ListRow r = ListRowAt(&s, rowIx);
    utassert(r.kind == ListRowKind::Entry && r.entry == 4);
    utassert(r.Path() == IndexPathNew(2).Section(2));
}

void TestIndexPath() {
    TestSuite("index_path");
    TheBuildersSetOnePartEach();
    ThePathNamesItsElement();
    AnEntryAndAPathAreTheSamePlace();
}
