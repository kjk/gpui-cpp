/* Themed tiles — crates/ui/src/dock/tiles.rs

   Tiles renders a TilesState: every tile is a floating panel with a drag bar
   along its top and a grab strip along each edge, drawn in z-order over the
   area. */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// One panel the host handed the tiles, which Rust holds as a TabPanel.
struct TilePanelDef {
    Str title = {};
    El* content = nullptr;
};

struct Tiles {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<TilesState> state = {};
    TilePanelDef panels[kMaxTiles] = {};
    int n = 0;

    static Tiles* New(Ctx* cx, Str id, Entity<TilesState> state);
    // The panel for the tile at that index. A tile with no panel draws its
    // frame and nothing inside it.
    Tiles* Panel(Str title, El* content);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
