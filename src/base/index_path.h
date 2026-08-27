#ifndef GPUI_BASE_INDEX_PATH_H_
#define GPUI_BASE_INDEX_PATH_H_
/* IndexPath — crates/base/src/index_path.rs

   How Rust addresses a row of a sectioned list: the section, the row inside
   it, and a column. Every list-shaped state upstream is keyed on one —
   `ListState`, `SearchableListState`, `ComboboxState`, the completion menu —
   where this tree keys on the flat entry index and keeps the section counts
   beside it.

   The two are the same information, and `ListIndexPathOf` / `ListEntryOf` in
   `base/list.h` convert between them, so a caller that thinks in sections has
   the type Rust gives it without every state having to carry one. */

#include "gpui/gpui.h"

namespace gpui {

struct IndexPath {
    int section = 0;
    int row = 0;
    int column = 0;

    // The builders. Rust's are `IndexPath::new(row).section(s).column(c)`,
    // which take self by value and hand it back.
    IndexPath Section(int v) const { return IndexPath{v, row, column}; }
    IndexPath Row(int v) const { return IndexPath{section, v, column}; }
    IndexPath Column(int v) const { return IndexPath{section, row, v}; }

    // eq_row: the same place in the list, whatever column either names.
    bool EqRow(IndexPath o) const {
        return section == o.section && row == o.row;
    }
};

// IndexPath::new(row): section and column default to zero.
inline IndexPath IndexPathNew(int row) {
    return IndexPath{0, row, 0};
}

inline bool operator==(IndexPath a, IndexPath b) {
    return a.section == b.section && a.row == b.row && a.column == b.column;
}
inline bool operator!=(IndexPath a, IndexPath b) {
    return !(a == b);
}

// `impl From<IndexPath> for ElementId`: "index-path(section,row,column)",
// which is what a row element is named by.
Str IndexPathIdStr(Arena* a, IndexPath p);
// The same string hashed the way every other element id here is, which is
// what `El::Click` takes.
uint32_t IndexPathClickId(IndexPath p);

} // namespace gpui
#endif // GPUI_BASE_INDEX_PATH_H_
