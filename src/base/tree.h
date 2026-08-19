/* Unstyled tree — crates/base/src/tree.rs */

#include "gpui/gpui.h"

namespace gpui {

// What a keystroke asks a tree to do. Rust binds up, down, left and right in
// the tree's key context; left and right are the folder pair, and each only
// acts in one direction.
enum class TreeAction : uint8_t {
    None,
    SelectPrev,
    SelectNext,
    Collapse,
    Expand
};

TreeAction TreeActionForKey(int key);

// Where Up and Down move the selection. Both wrap, and both treat an unset
// selection as 0 before stepping — which is why Up from nothing lands on the
// last entry while Down from nothing lands on the second. Rust's
// `checked_sub(1).unwrap_or(len - 1)` against its `if ix + 1 < len`.
// `selected` is -1 for unset.
int TreeSelectPrev(int selected, int count);
int TreeSelectNext(int selected, int count);

// Left collapses an expanded folder and does nothing else; Right expands a
// collapsed one. Neither touches a leaf, and neither is a toggle.
bool TreeCollapses(bool isFolder, bool isExpanded);
bool TreeExpands(bool isFolder, bool isExpanded);

struct Tree {
    static El* New(Ctx* cx);
};
// on_entry_click selects the entry and toggles it, so a press on a folder's
// row opens it and a press on a leaf just selects.
struct TreeItem {
    static El* New(Ctx* cx, Str id = {}, Listener onClick = {});
};
} // namespace gpui
