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
    // The rows fill the tree: Rust's item is mx_1 inside a size_full list, so
    // the selected background runs the width of the box less 4px a side.
    El* list = Div(a)->FlexCol()->W(kFill)->PadX(4)->PadY(4);
    for (int i = 0; i < 8; i++) {
        if (!Visible(app, i)) {
            continue;
        }
        int depth = Depth(i);
        bool sel = app->treeSel == i;
        El* row = TreeItem::New(cx, ClickTree + i)
                      ->H(32)
                      ->W(kFill)
                      ->PadX(8)
                      ->ItemsCenter()
                      ->Gap(4)
                      ->HoverBg(Rgb(0xf5, 0xf5, 0xf5))
                      ->OnClick(Listen(cx, &PickTree, i));
        if (sel) {
            row->Bg(Rgb(0xf0, 0xf0, 0xf0));
        }
        if (depth) {
            row->Child(Div(a)->W((float)depth * 12));
        }
        // A 12px chevron, right when the folder is collapsed and down when it
        // is open — the two SVGs the Rust example inlines. The box is there
        // for a file too, so the labels line up.
        El* icon = Div(a)->W(12)->H(12)->ItemsCenter()->JustifyCenter();
        if (kTree[i].folder) {
            IconName chevron = app->treeOpen[i] ? IconName::ChevronDown
                                                : IconName::ChevronRight;
            icon->Child(IconEl(a, chevron, 12)->Fg(ScInk()));
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

SHOWCASE_PAGE(CompTree, ShowcaseTree);
