#include "ui/tree.h"

namespace gpui {

namespace component {

Tree* Tree::New(Ctx* cx) {
    Arena* a = cx->a;
    Tree* t = ArenaNew<Tree>(a);
    t->a = a;
    t->cx = cx;
    return t;
}
Tree* Tree::Node(Str label, int parent, bool folder, bool open) {
    if (n < 16) {
        nodes[n].label = label;
        nodes[n].parent = parent;
        nodes[n].folder = folder;
        nodes[n].open = open;
        n++;
    }
    return this;
}
Tree* Tree::Selected(int i) {
    selected = i;
    return this;
}
Tree* Tree::OnSelect(Listener fn) {
    onSelect = fn;
    return this;
}

static bool Visible(Tree* t, int i) {
    int p = t->nodes[i].parent;
    while (p >= 0) {
        if (!t->nodes[p].open) {
            return false;
        }
        p = t->nodes[p].parent;
    }
    return true;
}

static int Depth(Tree* t, int i) {
    int d = 0;
    int p = t->nodes[i].parent;
    while (p >= 0) {
        d++;
        p = t->nodes[p].parent;
    }
    return d;
}

El* Tree::IntoEl() {
    const Theme& th = cx->theme();
    El* list = Div(a)->FlexCol();
    for (int i = 0; i < n; i++) {
        if (!Visible(this, i)) {
            continue;
        }
        El* row = TreeItem::New(cx, nodes[i].label)
                      ->H(28)
                      ->PadX(8)
                      ->ItemsCenter()
                      ->Gap(4);
        if (i == selected) {
            row->Bg(th.muted);
        }
        int d = Depth(this, i);
        if (d) {
            row->Child(Div(a)->W((float)d * 12));
        }
        if (nodes[i].folder) {
            row->Child(IconEl(a,
                              nodes[i].open ? IconName::ChevronDown
                                            : IconName::ChevronRight,
                              12)
                           ->Fg(th.mutedFg));
        } else {
            row->Child(Div(a)->W(12));
        }
        row->Child(TextEl(a, nodes[i].label)->Font(13)->Fg(th.foreground));
        if (onSelect.IsValid()) {
            row->OnClick(ListenerArg(onSelect, i));
        }
        list->Child(row);
    }
    return gpui::Tree::New(cx)
        ->W(256)
        ->H(192)
        ->Border(1, th.border)
        ->ClipY()
        ->Child(list);
}

} // namespace component
} // namespace gpui
