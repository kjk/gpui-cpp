#include "component/Table.h"

namespace component {

Table* Table::New(Arena* a) {
    Table* t = ::New<Table>(a);
    t->a = a;
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
    const Theme& th = ThemeNow();
    El* t = ::Table::New(a, StrL("table"))->FlexCol()->Border(1, th.border);
    El* head =
        TableHeader::New(a, StrL("th"))
            ->Child(TableRow::New(a, StrL("hr"))->FlexRow()->Bg(th.muted));
    for (int i = 0; i < nHeads; i++) {
        head->first->Child(
            TableHead::New(a, Str(heads[i]))
                ->Pad(8)
                ->Grow()
                ->Child(TextEl(a, Str(heads[i]))->Font(12)->Fg(th.mutedFg)));
    }
    t->Child(head);
    El* body = TableBody::New(a, StrL("tb"))->FlexCol();
    for (int r = 0; r < nRows; r++) {
        El* row = TableRow::New(a, str::Dup(a, fmt("r%d", r)))
                      ->FlexRow()
                      ->BorderT(1, th.border);
        for (int c = 0; c < nHeads; c++) {
            row->Child(TableCell::New(a, str::Dup(a, fmt("c%d", c)))
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
