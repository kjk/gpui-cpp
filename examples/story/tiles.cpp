#include "Story.h"

// crates/story/src/tiles_story.rs: panels that float over the area rather
// than splitting it, each moved by its bar and resized by its edges.
struct TilePanelData {
    const char* title;
    const char* body;
    Bounds bounds;
};

static TilePanelData kTiles[] = {
    {"Notes",
     "Drag this bar to move the tile. It snaps flush to a "
     "neighbour's edge, and to the top and left of the area.",
     {16, 16, 300, 200}},
    {"Chart",
     "Drag an edge to resize. An edge close to a neighbour's snaps "
     "to it; one close to nothing rounds to the 8px grid.",
     {340, 16, 260, 160}},
    {"Console",
     "The tile that was moved last comes to the front.",
     {16, 240, 400, 140}},
};

const int kNTiles = (int)(sizeof(kTiles) / sizeof(kTiles[0]));

struct TilesStory {
    Entity<TilesState> tiles = {};
    bool seeded = false;

    static El* Render(TilesStory* self, Ctx* cx);
    static void OnUndo(TilesStory* self, Ctx* cx, const ClickEvent* ev);
    static void OnRedo(TilesStory* self, Ctx* cx, const ClickEvent* ev);
};

void TilesStory::OnUndo(TilesStory* self, Ctx* cx, const ClickEvent*) {
    TilesState* s = self->tiles.Get(cx);
    if (s) {
        TilesUndo(s);
    }
    Notify(cx);
}

void TilesStory::OnRedo(TilesStory* self, Ctx* cx, const ClickEvent*) {
    TilesState* s = self->tiles.Get(cx);
    if (s) {
        TilesRedo(s);
    }
    Notify(cx);
}

El* TilesStory::Render(TilesStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->seeded) {
        self->seeded = true;
        self->tiles = EntityNewState<TilesState>(cx->app);
        TilesState* s = self->tiles.Get(cx);
        for (int i = 0; s && i < kNTiles; i++) {
            TilesAdd(s, i, kTiles[i].bounds);
        }
    }

    El* page = Div(a)->FlexCol()->Gap(16)->W(kFill);
    El* section = StorySection(
        cx, "Tiles",
        "Panels that float over the area: move one by its bar, resize it by "
        "an edge, and undo what the last drag did.");

    component::Tiles* tiles =
        component::Tiles::New(cx, StrL("story-tiles"), self->tiles);
    for (int i = 0; i < kNTiles; i++) {
        El* body = Div(a)->FlexCol()->Gap(8)->Pad(12)->W(kFill);
        body->Child(StoryTxt(cx, Str(kTiles[i].body), 13, th.mutedFg)->Wrap());
        tiles->Panel(Str(kTiles[i].title), body);
    }

    El* row = Div(a)->FlexRow()->Gap(8)->ItemsCenter();
    row->Child(component::Button::New(cx, StrL("tiles-undo"))
                   ->Label(StrL("Undo"))
                   ->Outline()
                   ->OnClick(Listen(cx, &TilesStory::OnUndo))
                   ->IntoEl());
    row->Child(component::Button::New(cx, StrL("tiles-redo"))
                   ->Label(StrL("Redo"))
                   ->Outline()
                   ->OnClick(Listen(cx, &TilesStory::OnRedo))
                   ->IntoEl());
    El* col = Div(a)->FlexCol()->Gap(12)->W(kFill);
    col->Child(row);
    col->Child(tiles->IntoEl()
                   ->W(kFill)
                   ->H(440)
                   ->Border(1, th.border)
                   ->Radius(th.radius)
                   ->Bg(th.secondary));
    StorySectionAdd(section, col);
    page->Child(section);
    return page;
}

STORY_PAGE(StoryTiles, TilesStory);
