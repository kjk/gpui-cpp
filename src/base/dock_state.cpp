#include "base/dock_state.h"

namespace gpui {

int DockAreaState::NewNode(Str panelName) {
    if (n >= kMaxPanelStateNodes) {
        return -1;
    }
    int ix = n++;
    nodes[ix] = PanelStateNode{};
    nodes[ix].panelName = panelName;
    return ix;
}

// Bounds, the way GPUI writes one: an origin and a size, each a pair.
static Bounds ParseBounds(const JsonValue* v) {
    Bounds b = {};
    const JsonValue* origin = JsonGet(v, "origin");
    const JsonValue* size = JsonGet(v, "size");
    b.x = (float)JsonNumber(JsonGet(origin, "x"));
    b.y = (float)JsonNumber(JsonGet(origin, "y"));
    b.w = (float)JsonNumber(JsonGet(size, "width"));
    b.h = (float)JsonNumber(JsonGet(size, "height"));
    return b;
}

static void WriteBounds(JsonWriter* w, const char* key, Bounds b) {
    w->BeginObject(key);
    w->BeginObject("origin");
    w->Number("x", b.x);
    w->Number("y", b.y);
    w->EndObject();
    w->BeginObject("size");
    w->Number("width", b.w);
    w->Number("height", b.h);
    w->EndObject();
    w->EndObject();
}

// One node and everything under it. Answers the node's index, or -1.
static int ParseNode(Arena* a, const JsonValue* v, DockAreaState* out) {
    if (!v || v->kind != JsonKind::Object) {
        return -1;
    }
    int ix = out->NewNode(StrDup(a, JsonString(JsonGet(v, "panel_name"))));
    if (ix < 0) {
        return -1;
    }
    // The children are read before the info, so a node's own fields are
    // written after the recursion has finished with the array.
    const JsonValue* children = JsonGet(v, "children");
    int childIx[kMaxPanelStateChildren];
    int nChild = 0;
    for (const JsonValue* c = children ? children->first : nullptr; c;
         c = c->next) {
        if (nChild >= kMaxPanelStateChildren) {
            break;
        }
        int child = ParseNode(a, c, out);
        if (child >= 0) {
            childIx[nChild++] = child;
        }
    }
    PanelStateNode& node = out->nodes[ix];
    node.nChild = nChild;
    for (int i = 0; i < nChild; i++) {
        node.children[i] = childIx[i];
    }

    // PanelInfo is an externally tagged enum: one member, named for the kind.
    const JsonValue* info = JsonGet(v, "info");
    const JsonValue* stack = JsonGet(info, "stack");
    const JsonValue* tabs = JsonGet(info, "tabs");
    const JsonValue* tiles = JsonGet(info, "tiles");
    if (stack) {
        node.kind = PanelInfoKind::Stack;
        const JsonValue* sizes = JsonGet(stack, "sizes");
        for (const JsonValue* s = sizes ? sizes->first : nullptr; s;
             s = s->next) {
            if (node.nSize >= kMaxPanelStateChildren) {
                break;
            }
            node.sizes[node.nSize++] = (float)JsonNumber(s);
        }
        // 0 is horizontal and 1 is vertical, which is what Rust writes.
        node.axis = (int)JsonNumber(JsonGet(stack, "axis")) == 0
                        ? Axis::Horizontal
                        : Axis::Vertical;
    } else if (tabs) {
        node.kind = PanelInfoKind::Tabs;
        node.activeIndex = (int)JsonNumber(JsonGet(tabs, "active_index"));
    } else if (tiles) {
        node.kind = PanelInfoKind::Tiles;
        const JsonValue* metas = JsonGet(tiles, "metas");
        for (const JsonValue* m = metas ? metas->first : nullptr; m;
             m = m->next) {
            if (node.nMeta >= kMaxPanelStateChildren) {
                break;
            }
            TileMeta& meta = node.metas[node.nMeta++];
            meta.bounds = ParseBounds(JsonGet(m, "bounds"));
            meta.zIndex = (int)JsonNumber(JsonGet(m, "z_index"));
        }
    } else {
        node.kind = PanelInfoKind::Panel;
        // Whatever the panel wrote is kept as it reads, so a round trip does
        // not lose what this port does not understand.
        const JsonValue* panel = JsonGet(info, "panel");
        if (panel && panel->kind == JsonKind::String) {
            node.info = StrDup(a, panel->str);
        }
    }
    return ix;
}

static DockPlacement PlacementOf(Str s) {
    if (StrEqI(s, StrL("left"))) {
        return DockPlacement::Left;
    }
    if (StrEqI(s, StrL("right"))) {
        return DockPlacement::Right;
    }
    if (StrEqI(s, StrL("bottom"))) {
        return DockPlacement::Bottom;
    }
    return DockPlacement::Center;
}

static const char* PlacementName(DockPlacement p) {
    switch (p) {
        case DockPlacement::Left:
            return "left";
        case DockPlacement::Right:
            return "right";
        case DockPlacement::Bottom:
            return "bottom";
        default:
            return "center";
    }
}

static void ParseDock(Arena* a, const JsonValue* v, DockAreaState* out,
                      DockSideState* side, DockPlacement fallback) {
    if (!v || v->kind != JsonKind::Object) {
        return;
    }
    side->present = true;
    side->node = ParseNode(a, JsonGet(v, "panel"), out);
    const JsonValue* placement = JsonGet(v, "placement");
    side->placement = placement ? PlacementOf(JsonString(placement)) : fallback;
    side->size = (float)JsonNumber(JsonGet(v, "size"));
    side->open = JsonBool(JsonGet(v, "open"), true);
}

bool DockAreaStateParse(Arena* a, Str json, DockAreaState* out) {
    *out = DockAreaState{};
    JsonValue* root = JsonParse(a, json);
    if (!root || root->kind != JsonKind::Object) {
        return false;
    }
    const JsonValue* version = JsonGet(root, "version");
    if (version && version->kind == JsonKind::Number) {
        out->hasVersion = true;
        out->version = (int)version->num;
    }
    out->center = ParseNode(a, JsonGet(root, "center"), out);
    ParseDock(a, JsonGet(root, "left_dock"), out, &out->left,
              DockPlacement::Left);
    ParseDock(a, JsonGet(root, "right_dock"), out, &out->right,
              DockPlacement::Right);
    ParseDock(a, JsonGet(root, "bottom_dock"), out, &out->bottom,
              DockPlacement::Bottom);
    return out->center >= 0;
}

static void WriteNode(JsonWriter* w, const char* key, const DockAreaState* s,
                      int ix) {
    if (ix < 0 || ix >= s->n) {
        w->Null(key);
        return;
    }
    const PanelStateNode& node = s->nodes[ix];
    w->BeginObject(key);
    w->String("panel_name", node.panelName);
    w->BeginArray("children");
    for (int i = 0; i < node.nChild; i++) {
        WriteNode(w, nullptr, s, node.children[i]);
    }
    w->EndArray();
    w->BeginObject("info");
    switch (node.kind) {
        case PanelInfoKind::Stack:
            w->BeginObject("stack");
            w->BeginArray("sizes");
            for (int i = 0; i < node.nSize; i++) {
                w->Number(nullptr, node.sizes[i]);
            }
            w->EndArray();
            w->Number("axis", AxisIsHorizontal(node.axis) ? 0 : 1);
            w->EndObject();
            break;
        case PanelInfoKind::Tabs:
            w->BeginObject("tabs");
            w->Number("active_index", node.activeIndex);
            w->EndObject();
            break;
        case PanelInfoKind::Tiles:
            w->BeginObject("tiles");
            w->BeginArray("metas");
            for (int i = 0; i < node.nMeta; i++) {
                w->BeginObject(nullptr);
                WriteBounds(w, "bounds", node.metas[i].bounds);
                w->Number("z_index", node.metas[i].zIndex);
                w->EndObject();
            }
            w->EndArray();
            w->EndObject();
            break;
        case PanelInfoKind::Panel:
        default:
            if (node.info.s) {
                w->String("panel", node.info);
            } else {
                w->Null("panel");
            }
            break;
    }
    w->EndObject();
    w->EndObject();
}

static void WriteDock(JsonWriter* w, const char* key, const DockAreaState* s,
                      const DockSideState& side) {
    if (!side.present) {
        // serde skips a dock that is not there rather than writing a null.
        return;
    }
    w->BeginObject(key);
    WriteNode(w, "panel", s, side.node);
    w->String("placement", Str(PlacementName(side.placement)));
    w->Number("size", side.size);
    w->Bool("open", side.open);
    w->EndObject();
}

void DockAreaStateWrite(const DockAreaState* s, StrBuilder* out) {
    JsonWriter w;
    w.out = out;
    w.BeginObject(nullptr);
    if (s->hasVersion) {
        w.Number("version", s->version);
    }
    WriteNode(&w, "center", s, s->center);
    WriteDock(&w, "left_dock", s, s->left);
    WriteDock(&w, "right_dock", s, s->right);
    WriteDock(&w, "bottom_dock", s, s->bottom);
    w.EndObject();
}

int TilesToMetas(const TilesState* s, TileMeta* out, int* outPanels, int cap) {
    int n = 0;
    for (int i = 0; i < s->n && n < cap; i++) {
        out[n].bounds = s->items[i].bounds;
        out[n].zIndex = s->items[i].zIndex;
        if (outPanels) {
            outPanels[n] = s->items[i].panel;
        }
        n++;
    }
    return n;
}

void TilesFromMetas(TilesState* s, const TileMeta* metas, const int* panels,
                    int n) {
    TileItem rebuilt[kMaxTiles] = {};
    int count = 0;
    for (int i = 0; i < n && count < kMaxTiles; i++) {
        int panel = panels ? panels[i] : i;
        // The tile showing that panel, wherever it has ended up in the list.
        int at = TilesIndexOfPanel(s, panel);
        rebuilt[count] = at >= 0 ? s->items[at] : TileItem{};
        rebuilt[count].panel = panel;
        rebuilt[count].bounds = metas[i].bounds;
        rebuilt[count].zIndex = metas[i].zIndex;
        count++;
    }
    // A tile the layout says nothing about keeps its place, after the ones it
    // does — the same as a panel the saved tree has no child for.
    for (int i = 0; i < s->n && count < kMaxTiles; i++) {
        bool saved = false;
        for (int k = 0; k < count; k++) {
            if (rebuilt[k].panel == s->items[i].panel) {
                saved = true;
                break;
            }
        }
        if (!saved) {
            rebuilt[count++] = s->items[i];
        }
    }
    for (int i = 0; i < count; i++) {
        s->items[i] = rebuilt[i];
    }
    s->n = count;
    s->dragging = -1;
    s->resizing = -1;
    s->side = TileSide::None;
}

} // namespace gpui
