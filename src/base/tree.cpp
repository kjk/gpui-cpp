#include "base/tree.h"
#include "base/actions.h"
#include "gpui/keymap.h"

namespace gpui {

Str TreeContext() {
    return StrL("Tree");
}

void TreeInitKeys() {
    static uint32_t bound = 0;
    if (bound == KeymapGeneration()) {
        return;
    }
    bound = KeymapGeneration();
    const char* ctx = "Tree";
    KeyBinding bindings[] = {
        {"up", action::SelectUp(), ctx},
        {"down", action::SelectDown(), ctx},
        {"left", action::SelectLeft(), ctx},
        {"right", action::SelectRight(), ctx},
        {"enter", action::Confirm(), ctx},
    };
    KeymapBind(bindings, (int)(sizeof(bindings) / sizeof(bindings[0])));
}

TreeAction TreeActionOf(uint32_t id) {
    if (id == action::SelectUp()) {
        return TreeAction::SelectPrev;
    }
    if (id == action::SelectDown()) {
        return TreeAction::SelectNext;
    }
    if (id == action::SelectLeft()) {
        return TreeAction::Collapse;
    }
    if (id == action::SelectRight()) {
        return TreeAction::Expand;
    }
    if (id == action::Confirm()) {
        return TreeAction::Confirm;
    }
    return TreeAction::None;
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

int TreeAddItem(TreeState* s, Str id, Str label, int parent) {
    if (s->nItems >= kMaxTreeItems) {
        return -1;
    }
    int ix = s->nItems++;
    TreeItem& it = s->items[ix];
    it = {};
    it.id = id;
    it.label = label;
    it.parent = parent;
    if (parent >= 0 && parent < ix) {
        it.depth = s->items[parent].depth + 1;
        // A folder is an item something else calls its parent.
        s->items[parent].folder = true;
    }
    return ix;
}

// add_entry: the item, then the children of an expanded one.
static void TreeAddEntry(TreeState* s, int item) {
    if (s->nEntries >= kMaxTreeItems) {
        return;
    }
    s->entries[s->nEntries++] = item;
    if (!s->items[item].expanded) {
        return;
    }
    for (int i = item + 1; i < s->nItems; i++) {
        if (s->items[i].parent == item) {
            TreeAddEntry(s, i);
        }
    }
}

void TreeRebuild(TreeState* s) {
    s->nEntries = 0;
    for (int i = 0; i < s->nItems; i++) {
        if (s->items[i].parent < 0) {
            TreeAddEntry(s, i);
        }
    }
    if (s->selected >= s->nEntries) {
        s->selected = s->nEntries > 0 ? s->nEntries - 1 : -1;
    }
    if (s->rightClicked >= s->nEntries) {
        s->rightClicked = -1;
    }
}

const TreeItem* TreeEntryItem(const TreeState* s, int entryIx) {
    if (entryIx < 0 || entryIx >= s->nEntries) {
        return nullptr;
    }
    return &s->items[s->entries[entryIx]];
}

int TreeIndexOf(const TreeState* s, Str id) {
    for (int i = 0; i < s->nEntries; i++) {
        if (StrSame(s->items[s->entries[i]].id, id)) {
            return i;
        }
    }
    return -1;
}

// cx.emit(TreeEvent::..) — see the note on ListEmit.
static void TreeEmit(TreeState* s, Ctx* cx, TreeEventKind kind, Str id,
                     int ix) {
    TreeEvent ev = {kind, id, ix};
    if (s->onEvent.IsValid()) {
        ListenerCall(cx->app, cx->win, s->onEvent, &ev);
    }
    EntityEmit(cx->app, cx->win, s->self, &ev);
}

bool TreeToggleExpandAt(TreeState* s, int entryIx, bool* expandedOut) {
    if (entryIx < 0 || entryIx >= s->nEntries) {
        return false;
    }
    TreeItem& it = s->items[s->entries[entryIx]];
    if (!it.folder) {
        return false;
    }
    it.expanded = !it.expanded;
    if (expandedOut) {
        *expandedOut = it.expanded;
    }
    // A right-click marks a row, and the rows move when a folder opens.
    s->rightClicked = -1;
    TreeRebuild(s);
    return true;
}

void TreeToggleExpand(TreeState* s, Ctx* cx, int entryIx) {
    if (entryIx < 0 || entryIx >= s->nEntries) {
        return;
    }
    Str id = s->items[s->entries[entryIx]].id;
    bool expanded = false;
    if (!TreeToggleExpandAt(s, entryIx, &expanded)) {
        return;
    }
    TreeEmit(s, cx,
             expanded ? TreeEventKind::Expanded : TreeEventKind::Collapsed, id,
             entryIx);
    Notify(cx);
}

int TreeRevealItem(TreeState* s, Str id) {
    int item = -1;
    for (int i = 0; i < s->nItems; i++) {
        if (StrSame(s->items[i].id, id)) {
            item = i;
            break;
        }
    }
    if (item < 0) {
        return -1;
    }
    for (int p = s->items[item].parent; p >= 0; p = s->items[p].parent) {
        s->items[p].expanded = true;
    }
    TreeRebuild(s);
    return TreeIndexOf(s, id);
}

void TreeScrollToItem(TreeState* s, int entryIx, ScrollStrategy strategy) {
    if (s->viewportH <= 0) {
        return;
    }
    s->scrollY = VirtualListScrollToRow(s->nEntries, s->rowH, entryIx,
                                        s->scrollY, s->viewportH, strategy);
}

void TreeSetSelected(TreeState* s, Ctx* cx, int entryIx) {
    s->selected = entryIx;
    Notify(cx);
}

void TreeClickEntry(TreeState* s, Ctx* cx, int entryIx) {
    const TreeItem* it = TreeEntryItem(s, entryIx);
    if (!it || it->disabled) {
        return;
    }
    s->selected = entryIx;
    TreeToggleExpand(s, cx, entryIx);
    Notify(cx);
}

void TreePerform(TreeState* s, Ctx* cx, TreeAction act) {
    const TreeItem* sel = TreeEntryItem(s, s->selected);
    switch (act) {
        case TreeAction::SelectPrev:
            s->selected = TreeSelectPrev(s->selected, s->nEntries);
            // Rust scrolls to the top for Up and the bottom for Down; both
            // strategies come down to the same "move as little as you can" in
            // the list, which is what keeps the row that arrived on screen.
            TreeScrollToItem(s, s->selected, ScrollStrategy::Top);
            break;
        case TreeAction::SelectNext:
            s->selected = TreeSelectNext(s->selected, s->nEntries);
            TreeScrollToItem(s, s->selected, ScrollStrategy::Bottom);
            break;
        case TreeAction::Collapse:
            if (sel && TreeCollapses(sel->folder, sel->expanded)) {
                TreeToggleExpand(s, cx, s->selected);
            }
            return;
        case TreeAction::Expand:
            if (sel && TreeExpands(sel->folder, sel->expanded)) {
                TreeToggleExpand(s, cx, s->selected);
            }
            return;
        case TreeAction::Confirm:
            if (sel && sel->folder) {
                TreeToggleExpand(s, cx, s->selected);
            }
            return;
        default:
            return;
    }
    Notify(cx);
}

void TreeState::OnRowClick(TreeState* self, Ctx* cx, const ClickEvent*,
                           intptr_t entryIx) {
    TreeClickEntry(self, cx, (int)entryIx);
}

void TreeState::OnRowMouseDown(TreeState* self, Ctx* cx,
                               const MouseDownEvent* ev, intptr_t entryIx) {
    if (ev->button != MouseButton::Right) {
        return;
    }
    const TreeItem* it = TreeEntryItem(self, (int)entryIx);
    if (!it || it->disabled) {
        return;
    }
    self->rightClicked = (int)entryIx;
    Notify(cx);
}

void TreeState::OnScroll(TreeState* self, Ctx* cx, const ScrollEvent* ev) {
    self->scrollY = ev->offsetY;
    Notify(cx);
}

El* Tree::New(Ctx* cx) {
    Arena* a = cx->a;
    return Div(a);
}

El* TreeItemEl::New(Ctx* cx, Str id, Listener onClick) {
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
void TreeOnAction(TreeState* self, Ctx* cx, const ActionEvent* ev) {
    if (!self) {
        return;
    }
    TreeAction act = TreeActionOf(ev->action);
    if (act == TreeAction::None) {
        const_cast<ActionEvent*>(ev)->propagate = true;
        return;
    }
    TreePerform(self, cx, act);
}

void TreeBindKeys(Ctx* cx, El* root, Entity<TreeState> state) {
    if (!cx || !root || !state.IsValid()) {
        return;
    }
    TreeInitKeys();
    Listener onAction = ListenTo(state, &TreeOnAction);
    root->KeyContext(TreeContext())
        ->OnAction(action::SelectUp(), onAction)
        ->OnAction(action::SelectDown(), onAction)
        ->OnAction(action::SelectLeft(), onAction)
        ->OnAction(action::SelectRight(), onAction)
        ->OnAction(action::Confirm(), onAction);
}

} // namespace gpui
