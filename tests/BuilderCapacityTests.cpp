/* Public component builders mirror Rust Vec-backed child lists. These checks
 * deliberately cross every capacity the port used to impose with a fixed
 * array; dropping the next child silently is a correctness bug. */

#include "Test.h"

using namespace gpui::component;

void TestBuilderCapacity() {
    TestSuite("builder capacity");
    Arena* a = ArenaNew();
    App app = {};
    Ctx cx = {};
    cx.a = a;
    cx.app = &app;

    AvatarGroup* avatars = AvatarGroup::New(&cx);
    DescriptionList* descriptions = DescriptionList::New(&cx);
    Form* form = Form::New(&cx);
    component::RadioGroup* radios =
        component::RadioGroup::Vertical(&cx, StrL("radios"));
    Stepper* stepper = Stepper::New(&cx, StrL("stepper"));
    AppMenuBar* menuBar = AppMenuBar::New(&cx, StrL("menus"), {});
    PieChart* pie = PieChart::New(&cx);
    float ys[2] = {0, 1};
    AreaChart* area = AreaChart::New(&cx, ys, 2);
    SidebarMenuItem* parent = SidebarMenuItem::New(&cx, StrL("parent"));
    SidebarMenu* menu = SidebarMenu::New(&cx);
    SidebarGroup* group = SidebarGroup::New(&cx, StrL("group"));
    Sidebar* sidebar = Sidebar::New(&cx, StrL("sidebar"));
    component::Accordion* accordion =
        component::Accordion::New(&cx, StrL("accordion"));
    Breadcrumb* breadcrumb = Breadcrumb::New(&cx);
    ButtonGroup* buttons = ButtonGroup::New(&cx, StrL("buttons"));
    DataTable* table = DataTable::New(&cx, StrL("table"), {});
    Settings* settings = Settings::New(&cx, StrL("settings"));
    settings->Page(StrL("page"))->Group(StrL("group"))->Item(
        StrL("setting"), StrL("description"), nullptr);
    TableGroupCell header = {StrL("group"), 1};

    for (int i = 0; i < 40; i++) {
        avatars->Child(component::Avatar::New(&cx));
        descriptions->Item(StrL("label"), StrL("value"));
        form->Field(StrL("label"), nullptr);
        radios->Child(StrL("radio"));
        stepper->Item(StepperItem::New(&cx));
        menuBar->Menu(StrL("menu"), nullptr);
        pie->Slice(1, RgbaHex(0x112233));
        area->Y(ys);
        parent->Child(SidebarMenuItem::New(&cx, StrL("child")));
        menu->Child(SidebarMenuItem::New(&cx, StrL("item")));
        group->Child(SidebarMenu::New(&cx));
        sidebar->Child(SidebarGroup::New(&cx, StrL("group")));
        accordion->Item(component::AccordionItem::New(&cx));
        breadcrumb->Child(StrL("level"));
        buttons->Child(component::Button::New(&cx, StrL("button")));
        table->GroupHeader(&header, 1);
        settings->Keyword(StrL("keyword"));
    }

    utassert(avatars->avatars.len == 40);
    utassert(descriptions->items.len == 40);
    utassert(form->fields.len == 40);
    utassert(radios->radios.len == 40);
    utassert(stepper->items.len == 40);
    utassert(menuBar->items.len == 40);
    utassert(pie->slices.len == 40);
    utassert(area->more.len == 40);
    utassert(parent->children.len == 40);
    utassert(menu->items.len == 40);
    utassert(group->menus.len == 40);
    utassert(sidebar->groups.len == 40);
    utassert(accordion->items.len == 40);
    utassert(breadcrumb->items.len == 40);
    utassert(buttons->children.len == 40);
    utassert(table->groupHeaders.len == 40);
    utassert(settings->pages[0].groups[0].items[0].keywords.len == 40);

    AppGlobalClear(&app);
    ArenaDelete(a);
}
