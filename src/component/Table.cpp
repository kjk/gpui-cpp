#include "component/Table.h"

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

} // namespace component
} // namespace gpui
