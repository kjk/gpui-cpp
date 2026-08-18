#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickTree = 580
};

struct TreeNode {
    const char* label;
    int parent; // -1 root
    bool folder;
};

static const TreeNode kTree[] = {
    {"src", -1, true},         {"components", 0, true},
    {"button.rs", 1, false},   {"tree.rs", 1, false},
    {"lib.rs", 0, false},      {"examples", -1, true},
    {"showcase.rs", 5, false}, {"Cargo.toml", -1, false},
};

static bool Visible(ShowcaseApp* app, int i) {
    int p = kTree[i].parent;
    while (p >= 0) {
        if (!app->treeOpen[p]) {
            return false;
        }
        p = kTree[p].parent;
    }
    return true;
}

static int Depth(int i) {
    int d = 0;
    int p = kTree[i].parent;
    while (p >= 0) {
        d++;
        p = kTree[p].parent;
    }
    return d;
}

static void PickTree(ShowcaseApp* app, Ctx* cx, const ClickEvent*, intptr_t i) {
    app->treeSel = (int)i;
    if (kTree[i].folder) {
        app->treeOpen[i] = !app->treeOpen[i];
    }
    Notify(cx);
}

El* ShowcaseTree(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* list = Div(a)->FlexCol()->PadY(4);
    for (int i = 0; i < 8; i++) {
        if (!Visible(app, i)) {
            continue;
        }
        int depth = Depth(i);
        bool sel = app->treeSel == i;
        El* row = TreeItem::New(cx, ClickTree + i)
                      ->H(32)
                      ->PadX(8)
                      ->ItemsCenter()
                      ->Gap(4)
                      ->HoverBg(Rgb(0xf5, 0xf5, 0xf5));
        if (sel) {
            row->Bg(Rgb(0xf0, 0xf0, 0xf0));
        }
        if (depth) {
            row->Child(Div(a)->W((float)depth * 12));
        }
        El* icon = Div(a)->W(12)->H(12)->ItemsCenter()->JustifyCenter();
        if (kTree[i].folder) {
            icon->Child(ScTxt(cx, app->treeOpen[i] ? StrL("v") : StrL(">"), 11,
                              ScInk()));
        }
        row->Child(icon);
        row->Child(ScTxt(cx, Str(kTree[i].label), 14, ScInk()));
        list->Child(row);
    }
    return Tree::New(cx)
        ->W(256)
        ->H(192)
        ->Border(1, Rgb(0xd4, 0xd4, 0xd4))
        ->ClipY()
        ->Child(list);
}

void ShowcaseTreeClick(ShowcaseApp* app, int id) {
    (void)app;
    (void)id;
}

SHOWCASE_PAGE(CompTree, ShowcaseTree, ShowcaseTreeClick);
