#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

El* ShowcaseTable(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    struct Row {
        const char* name;
        const char* status;
        const char* ver;
    };
    Row rows[] = {
        {"gpui-base", "Stable", "0.4.1"},
        {"gpui-component", "Active", "0.4.1"},
        {"story-web", "Preview", "0.2.8"},
        {"gpui-web", "Beta", "0.1.0"},
    };
    El* t = Table::New(cx, StrL("example-table"))
                ->FlexCol()
                ->W(288)
                ->Border(1, ExampleRgb(0xe5e7eb))
                ->ClipY();
    El* head =
        TableHeader::New(cx, StrL("header"))
            ->Child(TableRow::New(cx, StrL("header-row"))
                        ->FlexRow()
                        ->Bg(ExampleRgb(0xf5f5f5))
                        ->Child(TableHead::New(cx, StrL("name-head"))
                                    ->W(124)
                                    ->PadX(8)
                                    ->PadY(4)
                                    ->Child(TextEl(a, StrL("Component"))
                                                ->Font(12)
                                                ->Fg(ExampleRgb(0x171717))))
                        ->Child(TableHead::New(cx, StrL("status-head"))
                                    ->W(84)
                                    ->PadX(8)
                                    ->PadY(4)
                                    ->Child(TextEl(a, StrL("Status"))
                                                ->Font(12)
                                                ->Fg(ExampleRgb(0x171717))))
                        ->Child(TableHead::New(cx, StrL("version-head"))
                                    ->W(92)
                                    ->PadX(8)
                                    ->PadY(4)
                                    ->Child(TextEl(a, StrL("Version"))
                                                ->Font(12)
                                                ->Fg(ExampleRgb(0x171717)))));
    t->Child(head);
    El* body = TableBody::New(cx, StrL("body"))->FlexCol();
    for (int i = 0; i < 4; i++) {
        El* r = TableRow::New(cx, DupFmt(cx, "body-row-%d", i))
                    ->FlexRow()
                    ->BorderT(1, ExampleRgb(0xe5e7eb));
        r->Child(TableCell::New(cx, StrL("name"))
                     ->W(124)
                     ->PadX(8)
                     ->PadY(4)
                     ->Child(TextEl(a, Str(rows[i].name))
                                 ->Font(12)
                                 ->Fg(ExampleRgb(0x171717))));
        r->Child(TableCell::New(cx, DupFmt(cx, "status-%d", i))
                     ->W(84)
                     ->PadX(8)
                     ->PadY(4)
                     ->Child(Div(a)
                                 ->PadX(4)
                                 ->PadY(1)
                                 ->MinW(52)
                                 ->Border(1, ExampleRgb(0xd4d4d4))
                                 ->Child(TextEl(a, Str(rows[i].status))
                                             ->Font(12)
                                             ->Fg(ExampleRgb(0x171717)))));
        r->Child(TableCell::New(cx, DupFmt(cx, "version-%d", i))
                     ->W(92)
                     ->PadX(8)
                     ->PadY(4)
                     ->Child(TextEl(a, Str(rows[i].ver))
                                 ->Font(12)
                                 ->Fg(ExampleRgb(0x737373))));
        body->Child(r);
    }
    t->Child(body);
    return t;
}

SHOWCASE_PAGE(CompTable, ShowcaseTable);
