/* Themed tree — crates/ui/src/tree.rs

   crates/ui's Tree is the base tree with the gallery's row: an indent per
   depth, a chevron for a folder, an icon and a label. The rows come from a
   TreeState and only the visible ones are built, which is what makes it a
   virtualized tree rather than a list of every node. */

#include "ui/sizing.h"

namespace gpui {

namespace component {

struct Tree {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<TreeState> state = {};
    float h = 320;
    // Whether a row shows a file / folder icon beside its chevron.
    bool icons = true;

    static Tree* New(Ctx* cx, Str id, Entity<TreeState> state);
    Tree* H(float v);
    Tree* Icons(bool v);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
