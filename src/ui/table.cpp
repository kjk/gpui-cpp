#include "ui/table.h"
#include "ui/skeleton.h"

namespace gpui {

namespace component {

Table* Table::New(Ctx* cx) {
    Arena* a = cx->a;
    Table* t = ArenaNew<Table>(a);
    t->a = a;
    t->cx = cx;
    return t;
}
Table* Table::Heads(const char** h, int n) {
    heads = h;
    nHeads = n;
    return this;
}
Table* Table::Rows(const char*** r, int n) {
    rows = r;
    nRows = n;
    return this;
}

El* Table::IntoEl() {
    const Theme& th = cx->theme();
    El* t =
        gpui::Table::New(cx, StrL("table"))->FlexCol()->Border(1, th.border);
    El* head =
        TableHeader::New(cx, StrL("th"))
            ->Child(TableRow::New(cx, StrL("hr"))->FlexRow()->Bg(th.muted));
    for (int i = 0; i < nHeads; i++) {
        head->first->Child(
            TableHead::New(cx, Str(heads[i]))
                ->Pad(8)
                ->Grow()
                ->Child(TextEl(a, Str(heads[i]))->Font(12)->Fg(th.mutedFg)));
    }
    t->Child(head);
    El* body = TableBody::New(cx, StrL("tb"))->FlexCol();
    for (int r = 0; r < nRows; r++) {
        El* row = TableRow::New(cx, StrDup(a, fmt("r%d", r)))
                      ->FlexRow()
                      ->BorderT(1, th.border);
        for (int c = 0; c < nHeads; c++) {
            row->Child(TableCell::New(cx, StrDup(a, fmt("c%d", c)))
                           ->Pad(8)
                           ->Grow()
                           ->Child(TextEl(a, Str(rows[r][c]))
                                       ->Font(13)
                                       ->Fg(th.foreground)));
        }
        body->Child(row);
    }
    t->Child(body);
    return t;
}

DataTable* DataTable::New(Ctx* cx, Str id, Entity<TableState> state) {
    Arena* a = cx->a;
    DataTable* t = ArenaNew<DataTable>(a);
    t->a = a;
    t->cx = cx;
    t->id = id;
    t->state = state;
    return t;
}
DataTable* DataTable::Columns(const TableColumn* cols, int n) {
    columns = cols;
    nColumns = n;
    return this;
}
DataTable* DataTable::Rows(int n, void* d,
                           El* (*fn)(Ctx* cx, void* data, int row, int col)) {
    nRows = n;
    data = d;
    cell = fn;
    return this;
}
DataTable* DataTable::Stripe(bool v) {
    stripe = v;
    return this;
}
DataTable* DataTable::H(float px) {
    h = px;
    return this;
}
DataTable* DataTable::Empty(El* e) {
    empty = e;
    return this;
}
DataTable* DataTable::GroupHeader(El* el) {
    if (nGroupHeaders < 4 && el) {
        groupHeaders[nGroupHeaders++] = el;
    }
    return this;
}

// render_sort_icon: which way the column is sorted, and half-lit when it is
// not sorted at all. Rust has SortAscending / SortDescending glyphs; the two
// chevrons stand in for them here.
static El* SortIcon(Arena* a, const Theme& th, ColumnSort sort) {
    IconName name = IconName::ChevronsUpDown;
    bool on = true;
    switch (sort) {
        case ColumnSort::Ascending:
            name = IconName::ChevronUp;
            break;
        case ColumnSort::Descending:
            name = IconName::ChevronDown;
            break;
        default:
            on = false;
            break;
    }
    return Div(a)
        ->Pad(2)
        ->Radius(th.radius * 0.5f)
        ->HoverBg(th.secondary)
        ->Child(
            IconEl(a, name, 12)
                ->Fg(on ? th.secondaryFg : RgbaOpacity(th.secondaryFg, 0.5f)));
}

float DataTable::ColWidth(const TableState* s, int col) const {
    float declared = columns[col].width;
    return s ? TableColWidth(s, col, declared) : declared;
}

// render_resize_handle: a two-pixel grab straddling the column's right edge
// with a one-pixel line down it. Rust pulls the handle back over the edge with
// ml(-HANDLE_SIZE) because the head's content is w_full; the content here
// grows into whatever the handle leaves, which puts the same edge in the same
// place — and that is what lets the drag work out where the column starts.
static El* ResizeHandle(Ctx* cx, Str id, int col, Entity<TableState> state) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    TableState* s = state.Get(cx);
    bool on = s && s->resizingCol == col;
    // Nothing sets a height: a flex row stretches its children, so the handle
    // and the line down it are as tall as the head, which is what h_full says
    // in Rust.
    El* e = Div(a)
                ->Id(id)
                ->Click(HashClickId(id))
                ->FlexRow()
                ->W(kTableResizeHandleW)
                ->JustifyEnd()
                ->Cursor(CursorKind::ColResize);
    // group_hover: the line under the pointer, or under a drag, is the darker
    // of the two borders.
    e->Child(Div(a)->W(1)->Bg(on ? th.border : th.tableRowBorder));
    e->HoverBg(th.border);
    // on_drag(ResizeColumn((entity_id, ix))): the press picks the column up,
    // and every move afterwards carries it back to the handler.
    e->OnDrag(kTableResizeDrag, col);
    e->OnDragMove(ListenTo(state, &TableState::OnResizeDrag));
    // on_mouse_up_out ends the drag. A release back over the two pixels it
    // started on is not "out", so the handle listens for that one too — the
    // handler does nothing when no drag is going on.
    e->OnMouseUpOut(ListenTo(state, &TableState::OnResizeEnd));
    e->OnMouseUp(ListenTo(state, &TableState::OnResizeEnd));
    return e;
}

El* DataTable::IntoEl() {
    const Theme& th = cx->theme();
    TableState* s = state.Get(cx);
    if (s) {
        // The counts are the caller's every frame, which is what keeps the
        // keys inside the rows and columns there actually are.
        s->rowCount = nRows;
        s->colCount = nColumns;
        TableSeedColOrder(s, nColumns);
    }
    El* box = gpui::Table::New(cx, id)
                  ->FlexCol()
                  ->W(kFill)
                  ->Radius(th.radius)
                  ->Border(1, th.border);
    for (int i = 0; i < nGroupHeaders; i++) {
        box->Child(groupHeaders[i]);
    }

    Listener headClick = ListenTo(state, &TableState::OnHeadClick, 0);
    Listener sortClick = ListenTo(state, &TableState::OnSortClick, 0);
    El* head = TableHeader::New(cx, StrDup(a, fmt("%s-head", id)))
                   ->FlexRow()
                   ->W(kFill)
                   ->BorderB(1, th.border);
    for (int d = 0; d < nColumns; d++) {
        // The columns are drawn in the order the table keeps, which is what a
        // head drag rewrites.
        int c = s ? TableColAt(s, d) : d;
        const TableColumn& col = columns[c];
        // A column is as wide as the caller declared it until a drag has said
        // otherwise, from which point the width is the table's own.
        if (s) {
            TableSeedColWidth(s, c, col.width);
        }
        El* th_ = TableHead::New(cx, StrDup(a, fmt("%s-th-%d", id, c)))
                      ->FlexRow()
                      ->W(ColWidth(s, c));
        if (s && d < kMaxTableCols) {
            // The box a drop position is worked out against, which Rust reads
            // off its col_groups.
            th_->BoundsOut(&s->colBounds[d]);
        }
        if (d > 0) {
            th_->BorderL(1, th.border);
        }
        // The gap the dragged head would drop into, drawn down the edge it
        // would land on.
        if (s && s->dropGap == d) {
            th_->BorderL(2, th.primary);
        } else if (s && s->dropGap == d + 1 && d == nColumns - 1) {
            th_->BorderR(2, th.primary);
        }
        if (s && s->selectedCol == c && s->mode == TableSelectionMode::Column) {
            th_->Bg(th.accent);
        }
        // render_th: the head is the content and the resize handle beside it,
        // and only the content carries the padding — the handle has to reach
        // the column's edge.
        El* content = Div(a)
                          ->FlexRow()
                          ->Grow()
                          ->PadX(8)
                          ->PadY(6)
                          ->ItemsCenter()
                          ->JustifyBetween();
        content->Child(
            TextEl(a, col.title)->Font(14)->Fg(th.foreground)->LineHeight(1.f));
        if (col.selectable) {
            BindClick(content, StrDup(a, fmt("%s-th-%d", id, c)),
                      ListenerArg(headClick, c));
        }
        // on_drag(DragColumn(..)): a press on the head picks the column up,
        // and the whole head is the drop target for another one.
        if (s && s->colMovable) {
            content->OnDrag(kTableColDrag, d);
            content->OnDragMove(ListenTo(state, &TableState::OnColDragMove));
            content->OnMouseUpOut(ListenTo(state, &TableState::OnColDragEnd));
            content->OnMouseUp(ListenTo(state, &TableState::OnColDragEnd));
            th_->OnDrop(kTableColDrag, ListenTo(state, &TableState::OnColDrop));
        }
        if (col.sortable && s && s->sortable) {
            // The sort icon is its own hit box inside the head, so clicking it
            // sorts rather than selecting the column.
            El* icon = SortIcon(a, th, TableSortOf(s, c));
            BindClick(icon, StrDup(a, fmt("%s-sort-%d", id, c)),
                      ListenerArg(sortClick, c));
            content->Child(icon);
        }
        th_->Child(content);
        if (s && s->colResizable && col.resizable) {
            th_->Child(ResizeHandle(cx, StrDup(a, fmt("%s-resize-%d", id, c)),
                                    c, state));
        }
        head->Child(th_);
    }
    box->Child(head);

    // render_empty / render_loading: a table with no rows shows one of the
    // two instead of a body.
    if (s && s->loading) {
        El* skeleton = Div(a)->FlexCol()->W(kFill)->Gap(8)->Pad(12);
        for (int i = 0; i < 5; i++) {
            skeleton->Child(Skeleton::New(cx)->W(kFill)->H(16)->IntoEl());
        }
        box->Child(skeleton);
        return box;
    }
    if (nRows == 0) {
        box->Child(empty
                       ? empty
                       : Div(a)
                             ->FlexCol()
                             ->W(kFill)
                             ->H(h > 0 ? h : 160)
                             ->ItemsCenter()
                             ->JustifyCenter()
                             ->Child(IconEl(a, IconName::Inbox, 48)
                                         ->Fg(RgbaOpacity(th.mutedFg, 0.6f))));
        return box;
    }

    Listener rowClick = ListenTo(state, &TableState::OnRowClick, 0);
    Listener rowDown = ListenTo(state, &TableState::OnRowMouseDown, 0);
    El* body =
        TableBody::New(cx, StrDup(a, fmt("%s-body", id)))->FlexCol()->W(kFill);
    // The rows are virtualized when the caller gave the body a height: only
    // the ones it can show are built, with a spacer at each end standing in
    // for the rest. Without one every row is built, which is what a short
    // table wants.
    VirtualRange range = {0, nRows};
    if (s && h > 0) {
        s->viewportH = h;
        range = VirtualListVisibleRows(nRows, s->rowH, s->scrollY, h);
        body->H(h)
            ->ClipY()
            ->ScrollY(s->scrollY)
            ->ScrollId(HashClickId(id))
            ->OnScroll(ListenTo(state, &TableState::OnScroll));
        if (range.first > 0) {
            body->Child(Div(a)->W(kFill)->H((float)range.first * s->rowH));
        }
    }
    for (int r = range.first; r < range.end; r++) {
        El* row = TableRow::New(cx, StrDup(a, fmt("%s-row-%d", id, r)))
                      ->FlexRow()
                      ->W(kFill)
                      ->BorderB(1, th.tableRowBorder);
        if (s && h > 0) {
            // uniform_list: every row the same height, which is what lets the
            // two spacers stand in for the ones that were not built.
            row->H(s->rowH);
        }
        if (stripe && (r % 2) == 1) {
            row->Bg(th.tableEven);
        }
        if (s && s->selectedRow == r && s->mode == TableSelectionMode::Row) {
            row->Bg(th.accent);
        } else if (s && s->rightClickedRow == r) {
            row->Bg(RgbaOpacity(th.accent, 0.5f));
        }
        for (int d = 0; d < nColumns; d++) {
            int c = s ? TableColAt(s, d) : d;
            El* cellEl = cell ? cell(cx, data, r, c) : nullptr;
            El* td = TableCell::New(cx, StrDup(a, fmt("c%d", c)))
                         ->FlexRow()
                         ->W(ColWidth(s, c))
                         ->PadX(8)
                         ->PadY(6)
                         ->ItemsCenter();
            if (columns[c].right) {
                td->JustifyEnd();
            }
            if (d > 0) {
                td->BorderL(1, th.tableRowBorder);
            }
            if (s && s->mode == TableSelectionMode::Column &&
                s->selectedCol == c) {
                td->Bg(RgbaOpacity(th.accent, 0.5f));
            }
            if (cellEl) {
                // render_td clips what it holds to the column. Dragging an
                // edge in makes a column narrower than its text, and without
                // this the text would spill over the next column.
                float inner = ColWidth(s, c) - 16;
                cellEl->MaxW(inner > 1 ? inner : 1)->Truncate();
                td->Child(cellEl);
            }
            row->Child(td);
        }
        if (s && s->rowSelectable) {
            BindClick(row, StrDup(a, fmt("%s-row-%d", id, r)),
                      ListenerArg(rowClick, r));
            row->OnMouseDown(ListenerArg(rowDown, r));
        }
        body->Child(row);
    }
    if (s && h > 0 && range.end < nRows) {
        body->Child(Div(a)->W(kFill)->H((float)(nRows - range.end) * s->rowH));
    }
    box->Child(body);
    // load_more_if_need: the last row built is near the end, and the delegate
    // says there is more to come.
    if (s && TableShouldLoadMore(s, range.end) && s->onLoadMore.IsValid()) {
        TableEvent ev = {TableEventKind::SelectRow, s->rowCount, -1};
        ListenerCall(cx->app, cx->win, s->onLoadMore, &ev);
    }
    return box;
}

} // namespace component
} // namespace gpui
