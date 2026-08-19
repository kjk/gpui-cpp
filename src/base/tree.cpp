#include "base/tree.h"

namespace gpui {

TreeAction TreeActionForKey(int key) {
    switch (key) {
        case KeyUp:
            return TreeAction::SelectPrev;
        case KeyDown:
            return TreeAction::SelectNext;
        case KeyLeft:
            return TreeAction::Collapse;
        case KeyRight:
            return TreeAction::Expand;
        default:
            return TreeAction::None;
    }
}

int TreeSelectPrev(int selected, int count) {
    if (count <= 0) {
        return -1;
    }
    int ix = selected < 0 ? 0 : selected;
    return ix == 0 ? count - 1 : ix - 1;
}

int TreeSelectNext(int selected, int count) {
    if (count <= 0) {
        return -1;
    }
    int ix = selected < 0 ? 0 : selected;
    return ix + 1 < count ? ix + 1 : 0;
}

bool TreeCollapses(bool isFolder, bool isExpanded) {
    return isFolder && isExpanded;
}

bool TreeExpands(bool isFolder, bool isExpanded) {
    return isFolder && !isExpanded;
}

El* Tree::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}

El* TreeItem::New(Ctx* cx, Str id, Listener onClick) {
    Arena* a = cx->a;
    El* e = Div(a);
    if (id.s) {
        e->Id(id)->Click(HashClickId(id));
    }
    if (onClick.IsValid()) {
        e->OnClick(onClick);
    }
    return e;
}
} // namespace gpui
