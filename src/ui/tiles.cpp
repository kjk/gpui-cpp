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

Tiles* Tiles::Panel(Str title, El* content, El* suffix) {
    TilePanelDef d;
    d.title = title;
    d.content = content;
    d.suffix = suffix;
    panels.Append(a, d);
    return this;
}

Tiles* Tiles::Panel(PanelHandle panel, El* content) {
    TilePanelDef d;
    d.view = panel.IntoPanelView();
    d.hasView = true;
    d.title = d.view.title;
    d.content = content;
    panels.Append(a, d);
    return this;
}

Tiles* Tiles::WithSkin(const DockSkin* value) {
    skin = value;
    return this;
}

static El* TilePanelTitle(Ctx* cx, const TilePanelDef& panel, Rgba color) {
    if (panel.hasView && panel.view.titleEl) {
        if (El* title = panel.view.titleEl(cx, panel.view.data)) {
            return title;
        }
    }
    Str title = panel.title;
    if (!title.s && panel.hasView) {
        title = panel.view.name;
    }
    return TextEl(cx->a, title)->Fg(color);
}

static bool TilePanelTitleStyle(Ctx* cx, const TilePanelDef& panel,
                                TitleStyle* out) {
    if (!panel.hasView || !panel.view.titleStyle || !out) {
        return false;
    }
    return panel.view
        .titleStyle(cx, panel.view.data, &out->background, &out->foreground);
}

// One of the five grab strips: four along the edges, and the corner that
// takes the right and the bottom together.
static El* ResizeHandle(Ctx* cx, Entity<TilesState> st, int ix, int panel,
                        TileSide side, Bounds b) {
    Arena* a = cx->a;
    // Keyed by the panel rather than by the tile's place in the list, so
    // bringing a tile to the front does not renumber every element on it.
    Str hid = fmt("resize-%d-%d", panel, (int)side);
    El* e = Div(a)->PathClick(hid)->Absolute();
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
    DragResizing resizing = {packed};
    e->OnMouseDown(ListenTo(st, &TilesState::OnResizeDown, (intptr_t)packed));
    e->OnDrag(kTileResizeDrag, resizing.node);
    e->OnDragMove(ListenTo(st, &TilesState::OnResizeDrag));
    e->OnMouseUp(ListenTo(st, &TilesState::OnDragEnd));
    e->OnMouseUpOut(ListenTo(st, &TilesState::OnDragEnd));
    return e;
}

El* Tiles::IntoEl() {
    const Theme& th = ThemeNow(cx->app);
    TilesState* s = state.Get(cx);
    // The area names itself, so the drag bar and the resize strips on every
    // tile in it are named by the panel they belong to and nothing more.
    El* root = Div(a)->Id(id)->SizeFull()->ClipX()->ClipY();
    if (!s) {
        return root;
    }
    if (skin && skin->HasTilesScrollbarMode(cx->app)) {
        s->scrollbarMode = skin->GetTilesScrollbarMode(cx->app);
    }
    // The area scrolls over whatever the tiles cover: a tile dragged past an
    // edge is still reachable, which is what Rust's scroll_size says.
    Size content = TilesContentSize(s);
    root->ScrollX(s->scrollX)
        ->ScrollY(s->scrollY)
        ->ScrollMode(s->scrollbarMode)
        ->ScrollFromPath()
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
        // One extra pixel past the stored bounds, so a snapped neighbor's
        // border overlaps this tile's instead of stacking beside it into a
        // double-width line. Rust rides the growth on `min_w`/`min_h` because
        // base pins `w`/`h` to the stored bounds after the skin's hook; the
        // skin sizes the frame itself here, so the pixel goes on the size.
        //
        // No `overflow_hidden` here: the resize handles hang past the tile's
        // edge, and a content mask would cut their hit areas down to the
        // sliver inside it. The panel is clipped by its own body below.
        El* tile =
            Div(a)
                ->Absolute()
                ->Left(b.x)
                ->Top(b.y)
                ->W(b.w + 1)
                ->H(b.h + 1)
                ->FlexCol()
                ->Bg(th.tokens.background)
                ->Border(1, th.border)
                ->Radius(th.radius)
                // The frame hears the press its drag bar or one of its
                // resize handles took, on the way back out of the
                // chain, which is what brings it to the front.
                ->OnMouseDown(ListenTo(state, &TilesState::OnTileDown, ix))
                ->OnMouseUp(ListenTo(state, &TilesState::OnTileUp, ix));

        // The drag bar: the strip along the top that moves the tile.
        Str bid = fmt("bar-%d", item.panel);
        El* bar = Div(a)
                      ->PathClick(bid)
                      ->FlexRow()
                      ->ItemsCenter()
                      ->W(kFill)
                      ->H(kTileDragBarH)
                      ->Gap(4)
                      ->PadL(12)
                      ->PadR(8)
                      ->Bg(th.tokens.secondary)
                      ->Cursor(CursorKind::Arrow);
        // The panel is the caller's, named by the tile — the list is
        // reordered as tiles come to the front, and the panels are not.
        int p = item.panel;
        if (p >= 0 && p < panels.len) {
            TilePanelDef& panel = panels[p];
            TitleStyle titleStyle;
            bool hasStyle = TilePanelTitleStyle(cx, panel, &titleStyle);
            Rgba titleColor = hasStyle ? titleStyle.foreground : th.foreground;
            if (hasStyle) {
                // The tile frame does not clip its children, so a painted
                // title bar rounds its own top corners to stay inside the
                // frame's (`rounded_t(tile_radius)`).
                bar->Bg(titleStyle.background)
                    ->Fg(titleStyle.foreground)
                    ->Corners(th.tileRadius, th.tileRadius, 0, 0);
            }
            bar->Child(
                Div(a)
                    ->Flex1()
                    ->MinW(64)
                    ->ClipX()
                    ->Fg(titleColor)
                    ->Child(TilePanelTitle(cx, panel, titleColor)->Truncate()));
        }
        El* suffix = nullptr;
        if (p >= 0 && p < panels.len) {
            TilePanelDef& panel = panels[p];
            suffix = panel.suffix;
            if (!suffix && panel.hasView && panel.view.titleSuffix) {
                suffix = panel.view.titleSuffix(cx, panel.view.data);
            }
        }
        if (suffix) {
            // title_suffix sits at the far end of the bar, which is what the
            // flexible title before it makes room for.
            bar->Child(suffix->Shrink0());
        }
        if (p >= 0 && p < panels.len && panels[p].hasView &&
            panels[p].view.toolbarButtons) {
            if (El* tools = panels[p]
                                .view.toolbarButtons(cx, panels[p].view.data)) {
                bar->Child(tools->Shrink0());
            }
        }
        bar->OnMouseDown(
            ListenTo(state, &TilesState::OnMoveDown, (intptr_t)ix));
        DragMoving moving = {ix};
        bar->OnDrag(kTileMoveDrag, moving.node);
        bar->OnDragMove(ListenTo(state, &TilesState::OnMoveDrag));
        bar->OnMouseUp(ListenTo(state, &TilesState::OnDragEnd));
        bar->OnMouseUpOut(ListenTo(state, &TilesState::OnDragEnd));
        tile->Child(bar);

        El* body = Div(a)->FlexCol()->W(kFill)->H(kFill)->ClipX()->ClipY();
        if (p >= 0 && p < panels.len && panels[p].content) {
            body->Child(panels[p].content);
        } else if (p >= 0 && p < panels.len && panels[p].hasView &&
                   panels[p].view.render) {
            body->Child(panels[p].view.render(cx, panels[p].view.data));
        }
        tile->Child(body);

        const TileSide kSides[] = {TileSide::Left, TileSide::Right,
                                   TileSide::Top, TileSide::Bottom,
                                   TileSide::BottomRight};
        for (TileSide side : kSides) {
            tile->Child(ResizeHandle(cx, state, ix, p, side, b));
        }
        root->Child(tile);
    }
    return root;
}

} // namespace component
} // namespace gpui
