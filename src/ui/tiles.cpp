#include "ui/tiles.h"

namespace gpui {

namespace component {

Tiles* Tiles::New(Ctx* cx, Str id, Entity<TilesState> state) {
    Arena* a = cx->a;
    Tiles* t = ArenaNew<Tiles>(a);
    t->a = a;
    t->cx = cx;
    t->id = id;
    t->state = state;
    return t;
}

Tiles* Tiles::Panel(Str title, El* content) {
    TilePanelDef d;
    d.title = title;
    d.content = content;
    panels.Append(a, d);
    return this;
}

// One of the five grab strips: four along the edges, and the corner that
// takes the right and the bottom together.
static El* ResizeHandle(Ctx* cx, Str tid, Entity<TilesState> st, int ix,
                        int panel, TileSide side, Bounds b) {
    Arena* a = cx->a;
    // Keyed by the panel rather than by the tile's place in the list, so
    // bringing a tile to the front does not renumber every element on it.
    Str hid = fmt("%s-resize-%d-%d", tid, panel, (int)side);
    El* e = Div(a)->Id(hid)->Click(HashClickId(hid))->Absolute();
    switch (side) {
        case TileSide::Left:
            e->Left(0)->Top(0)->W(kTileHandleSize)->H(b.h);
            e->Cursor(CursorKind::ColResize);
            break;
        case TileSide::Right:
            e->Left(b.w - kTileHandleSize)->Top(0)->W(kTileHandleSize)->H(b.h);
            e->Cursor(CursorKind::ColResize);
            break;
        case TileSide::Top:
            e->Left(0)->Top(0)->W(b.w)->H(kTileHandleSize);
            e->Cursor(CursorKind::RowResize);
            break;
        case TileSide::Bottom:
            e->Left(0)->Top(b.h - kTileHandleSize)->W(b.w)->H(kTileHandleSize);
            e->Cursor(CursorKind::RowResize);
            break;
        case TileSide::BottomRight:
            // The anchor handle, which is a little square in the corner.
            e->Left(b.w - kTileHandleSize * 2)
                ->Top(b.h - kTileHandleSize * 2)
                ->W(kTileHandleSize * 2)
                ->H(kTileHandleSize * 2);
            e->Cursor(CursorKind::ColResize);
            break;
        case TileSide::None:
            return e;
    }
    int packed = TileResizePack(ix, side);
    e->OnMouseDown(ListenTo(st, &TilesState::OnResizeDown, (intptr_t)packed));
    e->OnDrag(kTileResizeDrag, packed);
    e->OnDragMove(ListenTo(st, &TilesState::OnResizeDrag));
    e->OnMouseUp(ListenTo(st, &TilesState::OnDragEnd));
    e->OnMouseUpOut(ListenTo(st, &TilesState::OnDragEnd));
    return e;
}

El* Tiles::IntoEl() {
    const Theme& th = cx->theme();
    TilesState* s = state.Get(cx);
    El* root = Div(a)->SizeFull()->ClipX()->ClipY();
    if (!s) {
        return root;
    }
    // The area scrolls over whatever the tiles cover: a tile dragged past an
    // edge is still reachable, which is what Rust's scroll_size says.
    Size content = TilesContentSize(s);
    root->ScrollX(s->scrollX)
        ->ScrollY(s->scrollY)
        ->ScrollMode(s->scrollbarMode)
        ->ScrollId(HashClickId(id))
        ->OnScroll(ListenTo(state, &TilesState::OnScroll));
    // The tiles are all out of flow, so nothing in the box has a size of its
    // own; this is what the scrollbars measure against.
    root->Child(Div(a)->W(content.w)->H(content.h));
    // The area's own box, so a pointer position in the window can be read in
    // the coordinates the tiles are laid out in.
    root->BoundsOut(&s->bounds);

    int* order = (int*)Alloc(a, (int)sizeof(int) * (s->items.len + 1));
    TilesPaintOrder(s, order);
    for (int k = 0; k < s->items.len; k++) {
        int ix = order[k];
        const TileItem& item = s->items[ix];
        Bounds b = item.bounds;
        // One pixel more, so two tiles pushed flush together do not leave a
        // seam where their borders meet.
        El* tile =
            Div(a)
                ->Absolute()
                ->Left(b.x)
                ->Top(b.y)
                ->W(b.w + 1)
                ->H(b.h + 1)
                ->FlexCol()
                ->Bg(th.background)
                ->Border(1, th.border)
                ->Radius(th.radius)
                ->ClipX()
                ->ClipY()
                // The frame hears the press its drag bar or one of its
                // resize handles took, on the way back out of the
                // chain, which is what brings it to the front.
                ->OnMouseDown(ListenTo(state, &TilesState::OnTileDown, ix))
                ->OnMouseUp(ListenTo(state, &TilesState::OnTileUp, ix));

        // The drag bar: the strip along the top that moves the tile.
        Str bid = fmt("%s-bar-%d", id, item.panel);
        El* bar = Div(a)
                      ->Id(bid)
                      ->Click(HashClickId(bid))
                      ->FlexRow()
                      ->ItemsCenter()
                      ->W(kFill)
                      ->H(kTileDragBarH)
                      ->PadX(8)
                      ->Bg(th.secondary)
                      ->Cursor(CursorKind::Arrow);
        // The panel is the caller's, named by the tile — the list is
        // reordered as tiles come to the front, and the panels are not.
        int p = item.panel;
        if (p >= 0 && p < panels.len && panels[p].title.s) {
            bar->Child(TextEl(a, panels[p].title)->Font(13)->Fg(th.foreground));
        }
        bar->OnMouseDown(
            ListenTo(state, &TilesState::OnMoveDown, (intptr_t)ix));
        bar->OnDrag(kTileMoveDrag, ix);
        bar->OnDragMove(ListenTo(state, &TilesState::OnMoveDrag));
        bar->OnMouseUp(ListenTo(state, &TilesState::OnDragEnd));
        bar->OnMouseUpOut(ListenTo(state, &TilesState::OnDragEnd));
        tile->Child(bar);

        El* body = Div(a)->FlexCol()->W(kFill)->H(kFill)->ClipX()->ClipY();
        if (p >= 0 && p < panels.len && panels[p].content) {
            body->Child(panels[p].content);
        }
        tile->Child(body);

        const TileSide kSides[] = {TileSide::Left, TileSide::Right,
                                   TileSide::Top, TileSide::Bottom,
                                   TileSide::BottomRight};
        for (TileSide side : kSides) {
            tile->Child(ResizeHandle(cx, id, state, ix, p, side, b));
        }
        root->Child(tile);
    }
    return root;
}

} // namespace component
} // namespace gpui
