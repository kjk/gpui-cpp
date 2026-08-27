#ifndef GPUI_SRC_UI_TILES_H_
#define GPUI_SRC_UI_TILES_H_
/* Themed tiles — crates/ui/src/dock/tiles.rs

   Tiles renders a TilesState: every tile is a floating panel with a drag bar
   along its top and a grab strip along each edge, drawn in z-order over the
   area. */

#include "ui/dock.h"

namespace gpui {

namespace component {

// The typed payloads used by the two tile drags. The runtime identifies drag
// families with kTileMoveDrag/kTileResizeDrag; these preserve Rust's payload
// shape and prevent the node id from becoming an anonymous integer at call
// sites.
struct DragMoving {
    int node = -1;
};

struct DragResizing {
    int node = -1;
};

// One panel the host handed the tiles, which Rust holds as a TabPanel.
struct TilePanelDef {
    Str title = {};
    El* content = nullptr;
    // Panel::title_suffix: what the panel puts at the far end of its own bar
    // — upstream's tiles example hangs a search field there. A DockPanelDef
    // has had one all along; this is the same hook on a tile.
    El* suffix = nullptr;
    PanelView view = {};
    bool hasView = false;
};

struct Tiles {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<TilesState> state = {};
    const DockSkin* skin = nullptr;
    // One panel per tile the caller adds; the builder is on the frame arena.
    ArenaVec<TilePanelDef> panels;

    static Tiles* New(Ctx* cx, Str id, Entity<TilesState> state);
    // The panel for the tile at that index. A tile with no panel draws its
    // frame and nothing inside it.
    Tiles* Panel(Str title, El* content, El* suffix = nullptr);
    Tiles* Panel(PanelHandle panel, El* content = nullptr);
    Tiles* WithSkin(const DockSkin* value);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
#endif // GPUI_SRC_UI_TILES_H_
