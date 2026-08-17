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
    TreeNode nodes[16] = {};
    int n = 0;
    int selected = -1;
    Func1<int> onSelect;

    static Tree* New(Arena* a);
    Tree* Node(Str label, int parent, bool folder, bool open);
    Tree* Selected(int i);
    Tree* OnSelect(Func1<int> fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
