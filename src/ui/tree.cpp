#include "ui/tree.h"

namespace gpui {

namespace component {

Tree* Tree::New(Ctx* cx, Str id, Entity<TreeState> state) {
    Arena* a = cx->a;
    Tree* t = ArenaNew<Tree>(a);
    t->a = a;
    t->cx = cx;
    t->id = id;
    t->state = state;
    return t;
}
Tree* Tree::H(float v) {
    h = v;
    return this;
}
Tree* Tree::Icons(bool v) {
    icons = v;
    return this;
}

// The themed row — crates/ui/src/tree.rs. tree_story.rs's row is an icon and
// a label and nothing else: File for a leaf, FolderOpen for an open folder
// and Folder for a shut one.
static El* TreeRow(void* user, Ctx* cx, int entryIx) {
    Tree* self = (Tree*)user;
    const Theme& th = cx->theme();
    TreeState* s = self->state.Get(cx);
    const TreeItem* it = s ? TreeEntryItem(s, entryIx) : nullptr;
    if (!it) {
        return nullptr;
    }
    Arena* a = cx->a;
    El* row = Div(a)
                  ->FlexRow()
                  ->W(kFill)
                  ->H(s->rowH)
                  // ListItem is px_3, and the story's row adds
                  // `pl(px(16.) * entry.depth() + px(12.))` on top of it,
                  // which is the whole of the indent: there is no spacer
                  // child and no chevron column.
                  ->PadR(12)
                  ->PadL(12 + (float)it->depth * 16)
                  ->Gap(8)
                  ->ItemsCenter()
                  ->Radius(th.radius);
    if (!it->disabled) {
        row->HoverBg(th.tokens.muted);
    }
    if (entryIx == s->selected) {
        row->Bg(th.tokens.accent);
    } else if (entryIx == s->rightClicked) {
        row->Bg(BackgroundOpacity(th.tokens.accent, 0.5f));
    }
    if (self->icons) {
        IconName ic = !it->folder    ? IconName::File
                      : it->expanded ? IconName::FolderOpen
                                     : IconName::Folder;
        row->Child(IconEl(a, ic, 16)
                       ->Fg(it->disabled ? th.mutedFg : th.foreground));
    }
    // ListItem is text_base, not text_sm.
    row->Child(TextEl(a, it->label)
                   ->Font(16)
                   ->Fg(it->disabled ? th.mutedFg : th.foreground));
    return row;
}

El* Tree::IntoEl() {
    if (!state.Get(cx)) {
        return Div(a)->H(h);
    }
    return TreeList::New(cx, id, state, h, &TreeRow, this);
}

} // namespace component
} // namespace gpui
