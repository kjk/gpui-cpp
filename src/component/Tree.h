/* Themed tree — crates/ui/src/tree.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct TreeNode {
    Str label = {};
    int parent = -1;
    bool folder = false;
    bool open = false;
};

struct Tree {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    TreeNode nodes[16] = {};
    int n = 0;
    int selected = -1;
    Listener onSelect;

    static Tree* New(Ctx* cx);
    Tree* Node(Str label, int parent, bool folder, bool open);
    Tree* Selected(int i);
    Tree* OnSelect(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
