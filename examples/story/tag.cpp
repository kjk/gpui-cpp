#include "Story.h"

static El* TagRow(Arena* a, StoryApp* app, bool outline, float radius) {
    Rgba indigo = Rgb(0x63, 0x66, 0xf1);
    Rgba indigoBg = Rgb(0xee, 0xf2, 0xff);
    const char* labels[] = {"Tag",     "Secondary", "Danger",
                            "Success", "Warning",   "Info"};
    using Fn = component::Tag* (*)(component::Tag*);
    Fn fns[] = {
        [](component::Tag* t) { return t->Primary(); },
        [](component::Tag* t) { return t->Secondary(); },
        [](component::Tag* t) { return t->Danger(); },
        [](component::Tag* t) { return t->Success(); },
        [](component::Tag* t) { return t->Warning(); },
        [](component::Tag* t) { return t->Info(); },
    };
    El* row = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->Wrap();
    for (int i = 0; i < 6; i++) {
        component::Tag* t = component::Tag::New(a, Str(labels[i]))
                                ->WithSize(app->size);
        fns[i](t);
        if (outline) {
            t->Outline();
        }
        if (radius >= 0) {
            t->Radius(radius);
        }
        row->Child(t->IntoEl());
    }
    if (radius < 0) {
        component::Tag* c =
            component::Tag::New(a, StrL("Custom"))
                ->Custom(outline ? ThemeNow().background : indigoBg, indigo)
                ->WithSize(app->size);
        if (outline) {
            c->Outline();
        }
        row->Child(c->IntoEl());
    }
    return row;
}

El* TagRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(a, app));

    El* def = StorySection(a, "Default", nullptr);
    StorySectionAdd(def, TagRow(a, app, false, -1));
    page->Child(def);

    El* out = StorySection(a, "Outline", nullptr);
    StorySectionAdd(out, TagRow(a, app, true, -1));
    page->Child(out);

    El* rnd = StorySection(a, "Rounded", nullptr);
    StorySectionAdd(rnd, TagRow(a, app, false, 99));
    page->Child(rnd);

    El* sq = StorySection(a, "Square", nullptr);
    StorySectionAdd(sq, TagRow(a, app, false, 0));
    page->Child(sq);

    El* colors = StorySection(a, "Colors", nullptr);
    El* crow = Div(a)->FlexRow()->Gap(8)->ItemsCenter()->Wrap();
    struct Named {
        const char* name;
        Rgba c;
    };
    const Theme& th = ThemeNow();
    Named named[] = {{"Red", th.red},         {"Green", th.green},
                     {"Blue", th.blue},       {"Yellow", th.yellow},
                     {"Info", th.info},       {"Success", th.success},
                     {"Warning", th.warning}, {"Danger", th.danger}};
    for (int i = 0; i < 8; i++) {
        crow->Child(component::Tag::New(a, Str(named[i].name))
                        ->Custom(RgbaOpacity(named[i].c, 0.15f), named[i].c)
                        ->WithSize(app->size)
                        ->IntoEl());
    }
    StorySectionAdd(colors, crow);
    page->Child(colors);
    return page;
}

void TagClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryTag, TagRender, TagClick);
