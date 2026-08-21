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

El* Tree::IntoEl() {
    const Theme& th = cx->theme();
    TreeState* s = state.Get(cx);
    if (!s) {
        return Div(a)->H(h);
    }
    s->self = state.id;
    // The height the list was laid out at is what scroll_to_item measures
    // against, and the caller is the one that knows it.
    s->viewportH = h;

    // uniform_list: only the rows the viewport can show are built, and the
    // two spacers stand in for the rest so the scrollbar spans the whole
    // tree.
    VirtualRange range =
        VirtualListVisibleRows(s->entries.len, s->rowH, s->scrollY, h);
    int first = range.first;
    int end = range.end;

    El* list = Div(a)->FlexCol()->W(kFill);
    if (first > 0) {
        list->Child(Div(a)->W(kFill)->H((float)first * s->rowH));
    }
    Listener click = ListenTo(state, &TreeState::OnRowClick, 0);
    Listener down = ListenTo(state, &TreeState::OnRowMouseDown, 0);
    for (int i = first; i < end; i++) {
        const TreeItem* it = TreeEntryItem(s, i);
        if (!it) {
            break;
        }
        bool on = i == s->selected;
        El* row = TreeItemEl::New(cx, StrDup(a, fmt("%s-row-%d", id, i)),
                                  ListenerArg(click, i))
                      ->FlexRow()
                      ->W(kFill)
                      ->H(s->rowH)
                      ->PadR(12)
                      ->PadL(12)
                      ->Gap(8)
                      ->ItemsCenter()
                      ->Radius(th.radius);
        if (!it->disabled) {
            row->HoverBg(th.tokens.muted);
            row->OnMouseDown(ListenerArg(down, i));
        }
        if (on) {
            row->Bg(th.tokens.accent);
        } else if (i == s->rightClicked) {
            row->Bg(BackgroundOpacity(th.tokens.accent, 0.5f));
        }
        // Every entry indents by its depth, so a child lines up under the
        // chevron of the folder it is in.
        if (it->depth > 0) {
            row->Child(Div(a)->W((float)it->depth * 16));
        }
        // The chevron box is there for a leaf too, so the labels line up.
        El* chevron = Div(a)->W(12)->H(12)->ItemsCenter()->JustifyCenter();
        if (it->folder) {
            chevron->Child(IconEl(a,
                                  it->expanded ? IconName::ChevronDown
                                               : IconName::ChevronRight,
                                  12)
                               ->Fg(th.mutedFg));
        }
        row->Child(chevron);
        if (icons) {
            row->Child(
                IconEl(a, it->folder ? IconName::Folder : IconName::File, 16)
                    ->Fg(it->disabled ? th.mutedFg : th.foreground));
        }
        row->Child(TextEl(a, it->label)
                       ->Font(14)
                       ->Fg(it->disabled ? th.mutedFg : th.foreground));
        list->Child(row);
    }
    if (end < s->entries.len) {
        list->Child(
            Div(a)->W(kFill)->H((float)(s->entries.len - end) * s->rowH));
    }

    El* box = gpui::Tree::New(cx)
                  ->FlexCol()
                  ->W(kFill)
                  ->H(h)
                  ->ClipY()
                  ->ScrollY(s->scrollY)
                  ->ScrollId(HashClickId(id))
                  ->OnScroll(ListenTo(state, &TreeState::OnScroll));
    box->Child(list);
    // The tree's own context and the four arrows in it. The rows are not
    // focusable, so the box is: Rust tracks focus on the same element it
    // declares the context on.
    box->FocusId(HashClickId(id))->FocusRing(false);
    TreeBindKeys(cx, box, state);
    return box;
}

} // namespace component
} // namespace gpui
