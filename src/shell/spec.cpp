#include "shell/spec.h"

namespace gpui::shell {

uint64_t StructureMix(uint64_t state, uint64_t value) {
    uint64_t rotated = (state << 23) | (state >> 41);
    uint64_t hashed = rotated ^ (value * 0x9E3779B97F4A7C15ull);
    hashed ^= hashed >> 30;
    hashed *= 0xBF58476D1CE4E5B9ull;
    return hashed ^ (hashed >> 27);
}

// A method name is identified by what it says rather than by where it is.
// Rust hashes the `&'static str`'s pointer, because upstream's reflection
// table hands the same pointer to every call of one builder method; here a
// name arrives copied into a per-call arena, so there is no pointer to lean
// on and the bytes are what separate two names.
static uint64_t StructureName(Str name) {
    uint64_t hashed = 0;
    for (int i = 0; i < name.len; i++) {
        hashed = StructureMix(hashed, (uint64_t)(uint8_t)name.s[i]);
    }
    return StructureMix(hashed, (uint64_t)name.len);
}

// What this node contributes to a fingerprint: which constructor produced it,
// and nothing it carries. A Text's string, a Button's id, a VirtualList's item
// count and a ChildView's handle are all *values* as far as a template is
// concerned.
static uint64_t ComponentShape(const Component& component) {
    return StructureMix(0, (uint64_t)component.kind);
}

// What this operation contributes: which call it was, and not what was passed
// to it. Arguments are left out because a colour, a length or a label changing
// is precisely the case a template exists to serve, and a CallbackId is left
// out because it is minted per render and retired with the snapshot
// generation, so keeping it would make every description containing a single
// handler look like a new shape.
static uint64_t OpShape(const SpecOp& op) {
    switch (op.kind) {
        case SpecOpKind::NullaryStyle:
            return StructureMix(1, StructureName(op.name));
        case SpecOpKind::ParamStyle:
            return StructureMix(2, StructureName(op.name));
        case SpecOpKind::Method:
            return StructureMix(3, StructureName(op.name));
        case SpecOpKind::Callback:
            return StructureMix(4, StructureName(op.name));
        // The detached node these point at is part of the shape: a hover style
        // and the declarations inside it are one structure.
        case SpecOpKind::StateStyle:
            return StructureMix(StructureMix(6, StructureName(op.name)),
                                op.node);
        case SpecOpKind::Slot:
            return StructureMix(StructureMix(7, StructureName(op.name)),
                                op.node);
    }
    return 0;
}

Template::~Template() {
    delete arena;
    VecReset(slots);
}

const char* ComponentName(const Component& component) {
    switch (component.kind) {
        case ComponentKind::Div:
            return "div";
        case ComponentKind::HFlex:
            return "h_flex";
        case ComponentKind::VFlex:
            return "v_flex";
        case ComponentKind::ChildView:
            return "child_view";
        case ComponentKind::Text:
            return "text";
        case ComponentKind::Button:
            return "Button";
        case ComponentKind::Link:
            return "Link";
        case ComponentKind::Checkbox:
            return "Checkbox";
        case ComponentKind::Switch:
            return "Switch";
        case ComponentKind::Scrollbar:
            return "Scrollbar";
        case ComponentKind::Input:
            return "Input";
        case ComponentKind::Textarea:
            return "Textarea";
        case ComponentKind::NumberInput:
            return "NumberInput";
        case ComponentKind::OtpInput:
            return "OtpInput";
        case ComponentKind::Svg:
            return "svg";
        case ComponentKind::Accordion:
            return "Accordion";
        case ComponentKind::AccordionItem:
            return "AccordionItem";
        case ComponentKind::AccordionHeader:
            return "AccordionHeader";
        case ComponentKind::AccordionPanel:
            return "AccordionPanel";
        case ComponentKind::AccordionTrigger:
            return "AccordionTrigger";
        case ComponentKind::Pagination:
            return "Pagination";
        case ComponentKind::Avatar:
            return "Avatar";
        case ComponentKind::AvatarImage:
            return "AvatarImage";
        case ComponentKind::AvatarFallback:
            return "AvatarFallback";
        case ComponentKind::Image:
            return "image";
        case ComponentKind::PathFill:
            return "path fill";
        case ComponentKind::PathStroke:
            return "path stroke";
        case ComponentKind::Tabs:
            return "Tabs";
        case ComponentKind::Tab:
            return "Tab";
        case ComponentKind::Progress:
            return "Progress";
        case ComponentKind::ProgressTrack:
            return "ProgressTrack";
        case ComponentKind::ProgressIndicator:
            return "ProgressIndicator";
        case ComponentKind::FpsMonitor:
            return "FpsMonitor";
        case ComponentKind::Slider:
            return "Slider";
        case ComponentKind::SliderTrack:
            return "SliderTrack";
        case ComponentKind::SliderIndicator:
            return "SliderIndicator";
        case ComponentKind::SliderThumb:
            return "SliderThumb";
        case ComponentKind::Radio:
            return "Radio";
        case ComponentKind::Toggle:
            return "Toggle";
        case ComponentKind::RadioGroup:
            return "RadioGroup";
        case ComponentKind::ToggleGroup:
            return "ToggleGroup";
        case ComponentKind::Table:
            return "Table";
        case ComponentKind::TableHeader:
            return "TableHeader";
        case ComponentKind::TableBody:
            return "TableBody";
        case ComponentKind::TableRow:
            return "TableRow";
        case ComponentKind::TableHead:
            return "TableHead";
        case ComponentKind::TableCell:
            return "TableCell";
        case ComponentKind::TableCaption:
            return "TableCaption";
        case ComponentKind::HResizable:
            return "h_resizable";
        case ComponentKind::VResizable:
            return "v_resizable";
        case ComponentKind::ResizablePanel:
            return "resizable_panel";
        case ComponentKind::Collapsible:
            return "Collapsible";
        case ComponentKind::Popover:
            return "Popover";
        case ComponentKind::HoverCard:
            return "HoverCard";
        case ComponentKind::Popup:
            return "Popup";
        case ComponentKind::Select:
            return "Select";
        case ComponentKind::Combobox:
            return "Combobox";
        case ComponentKind::DatePicker:
            return "DatePicker";
        case ComponentKind::DockArea:
            return "dock_area";
        case ComponentKind::DockContent:
            return "dock_content";
        case ComponentKind::VVirtualList:
            return "v_virtual_list";
        case ComponentKind::HVirtualList:
            return "h_virtual_list";
    }
    return "element";
}

static void SetSpecError(SpecError* error, SpecErrorKind kind,
                         Str component = {}) {
    if (!error) return;
    error->kind = kind;
    error->component = component;
}

Str SpecErrorMessage(Arena* arena, const SpecError& error) {
    switch (error.kind) {
        case SpecErrorKind::None:
            return {};
        case SpecErrorKind::Claimed:
            return StrDup(
                arena,
                StrL("this element was given to a method that takes one — a "
                     "state style's declarations, or a named slot such as "
                     "content — and cannot also be added to the tree"));
        case SpecErrorKind::Expired:
            return StrDup(arena,
                          StrL("this element belongs to a previous render "
                               "pass; elements are single-use values and must "
                               "be rebuilt each time render runs"));
        case SpecErrorKind::AlreadyParented:
            return StrDup(arena, fmt("element `%s` was already added to a "
                                     "parent; elements are single-use values",
                                     error.component));
        case SpecErrorKind::SelfParent:
            return StrDup(arena, StrL("an element cannot be added to itself"));
        case SpecErrorKind::DuplicateChildView:
            return StrDup(
                arena,
                StrL("a child view handle can be mounted only once in one "
                     "snapshot; create a second Entity for a second position"));
    }
    return {};
}

SpecArena::SpecArena() {
    arena = ArenaNew();
}

SpecArena::~SpecArena() {
    ArenaDelete(arena);
}

void SpecArena::Reset() {
    VecClear(nodes);
    VecClear(parented);
    VecClear(claimed);
    VecClear(mountedViews);
    virtualItems = 0;
    structure = 0;
    arena->Reset();
}

Component SpecArena::CopyComponent(const Component& source) {
    Component out = source;
    out.text = StrDup(arena, source.text);
    out.background.color = StrDup(arena, source.background.color);
    out.background.fromColor = StrDup(arena, source.background.fromColor);
    out.background.toColor = StrDup(arena, source.background.toColor);
    out.background.colorSpace = StrDup(arena, source.background.colorSpace);
    if (source.virtualList) {
        out.virtualList = ArenaNew<VirtualListSpec>(arena);
        *out.virtualList = *source.virtualList;
        out.virtualList->id = StrDup(arena, source.virtualList->id);
        if (source.virtualList->sizeCount > 0) {
            out.virtualList->sizes = (Size*)Alloc(
                arena,
                (int)(sizeof(Size) * (size_t)source.virtualList->sizeCount));
            memcpy(out.virtualList->sizes, source.virtualList->sizes,
                   sizeof(Size) * (size_t)source.virtualList->sizeCount);
        }
    }
    return out;
}

SpecOp SpecArena::CopyOp(const SpecOp& source) {
    SpecOp out = source;
    out.name = StrDup(arena, source.name);
    if (source.argCount > 0) {
        out.args = (Bridged*)Alloc(
            arena, (int)(sizeof(Bridged) * (size_t)source.argCount));
        for (int i = 0; i < source.argCount; i++) {
            out.args[i] = source.args[i];
            if (out.args[i].kind == BridgedKind::String) {
                out.args[i].string = StrDup(arena, source.args[i].string);
            }
        }
    }
    return out;
}

SpecId SpecArena::Push(const Component& component) {
    structure = StructureMix(structure, ComponentShape(component));
    SpecNode* node = ArenaNew<SpecNode>(arena);
    node->component = CopyComponent(component);
    VecAppend(nodes, node);
    VecAppend(parented, 0);
    VecAppend(claimed, 0);
    return (SpecId)(nodes.len - 1);
}

bool SpecArena::PushChildView(const Component& component, SpecId* out,
                              SpecError* error) {
    if (component.kind != ComponentKind::ChildView) {
        SetSpecError(error, SpecErrorKind::Expired);
        return false;
    }
    for (int i = 0; i < mountedViews.len; i++) {
        if (mountedViews[i] == component.handle) {
            SetSpecError(error, SpecErrorKind::DuplicateChildView);
            return false;
        }
    }
    VecAppend(mountedViews, component.handle);
    SpecId id = Push(component);
    if (out) *out = id;
    return true;
}

bool SpecArena::PushDockArea(uint64_t handle, SpecId* out, SpecError* error) {
    for (int i = 0; i < mountedViews.len; i++) {
        if (mountedViews[i] == handle) {
            SetSpecError(error, SpecErrorKind::DuplicateChildView);
            return false;
        }
    }
    VecAppend(mountedViews, handle);
    Component component;
    component.kind = ComponentKind::DockArea;
    component.handle = handle;
    SpecId id = Push(component);
    if (out) *out = id;
    return true;
}

const SpecNode* SpecArena::Node(SpecId id) const {
    return id < (SpecId)nodes.len ? nodes[(int)id] : nullptr;
}

bool SpecArena::CheckLive(SpecId id, SpecError* error) const {
    if (id >= (SpecId)nodes.len || !nodes[(int)id]) {
        SetSpecError(error, SpecErrorKind::Expired);
        return false;
    }
    if (parented[(int)id]) {
        SetSpecError(error, SpecErrorKind::AlreadyParented,
                     Str(ComponentName(nodes[(int)id]->component)));
        return false;
    }
    return true;
}

bool SpecArena::PushOp(SpecId id, const SpecOp& op, SpecError* error) {
    if (!CheckLive(id, error)) return false;
    // After the check, not before: a rejected call records nothing, so it must
    // not move the shape either.
    structure = StructureMix(structure, OpShape(op));
    return nodes[(int)id]->ops.Append(arena, CopyOp(op));
}

bool SpecArena::Claim(SpecId id, SpecError* error) {
    if (!CheckLive(id, error)) return false;
    if (claimed[(int)id]) {
        SetSpecError(error, SpecErrorKind::Claimed);
        return false;
    }
    structure = StructureMix(structure, StructureMix(8, id));
    claimed[(int)id] = 1;
    return true;
}

bool SpecArena::Attach(SpecId parent, SpecId child, SpecError* error) {
    if (!CheckLive(parent, error) || !CheckLive(child, error)) return false;
    if (claimed[(int)child]) {
        SetSpecError(error, SpecErrorKind::Claimed);
        return false;
    }
    if (parent == child) {
        SetSpecError(error, SpecErrorKind::SelfParent);
        return false;
    }
    // The tree is the half of the shape the nodes themselves do not carry: the
    // same components in a different arrangement are a different structure,
    // and a template could not fill one from the other.
    structure = StructureMix(StructureMix(structure, parent),
                             (uint64_t)child ^ (1ull << 32));
    parented[(int)child] = 1;
    return nodes[(int)parent]->children.Append(arena, child);
}

SpecId SpecArena::Graft(const Template& tmpl) {
    SpecId base = (SpecId)nodes.len;
    const SpecArena* source = tmpl.arena;
    if (!source) return base;
    for (int i = 0; i < source->nodes.len; i++) {
        const SpecNode* from = source->nodes[i];
        SpecNode* node = ArenaNew<SpecNode>(arena);
        node->component = CopyComponent(from->component);
        for (const SpecOp& op : from->ops) {
            SpecOp copied = CopyOp(op);
            if (copied.kind == SpecOpKind::StateStyle ||
                copied.kind == SpecOpKind::Slot) {
                copied.node += base;
            }
            node->ops.Append(arena, copied);
        }
        for (SpecId child : from->children) {
            node->children.Append(arena, child + base);
        }
        VecAppend(nodes, node);
        VecAppend(parented, source->parented[i]);
        VecAppend(claimed, source->claimed[i]);
    }
    // The root arrives with no parent so the caller can attach it, whatever it
    // was in the template.
    parented[(int)(tmpl.root + base)] = 0;

    // One mix for the whole graft rather than one per node: the template's own
    // fingerprint already summarizes everything inside it, and a description
    // that instantiates the same template twice must not look like one that
    // instantiated two different ones.
    structure = StructureMix(structure, StructureMix(9, source->structure));
    return tmpl.root + base;
}

bool SpecArena::WriteSlot(SpecId base, const Slot& slot, const SlotValue& value,
                          SpecError* error) {
    SpecId target = slot.node + base;
    if (target >= (SpecId)nodes.len || !nodes[(int)target]) {
        SetSpecError(error, SpecErrorKind::Expired);
        return false;
    }
    SpecNode* node = nodes[(int)target];
    if (slot.site.kind == SlotSiteKind::Text &&
        value.kind == SlotValueKind::Text) {
        node->component.kind = ComponentKind::Text;
        node->component.text = StrDup(arena, value.text);
        return true;
    }
    if (slot.site.kind == SlotSiteKind::Argument &&
        value.kind == SlotValueKind::Value) {
        if ((int)slot.site.op >= node->ops.len) {
            SetSpecError(error, SpecErrorKind::Expired);
            return false;
        }
        SpecOp& op = node->ops[slot.site.op];
        if (op.kind != SpecOpKind::ParamStyle &&
            op.kind != SpecOpKind::Method) {
            SetSpecError(error, SpecErrorKind::Expired);
            return false;
        }
        if ((int)slot.site.argument >= op.argCount) {
            SetSpecError(error, SpecErrorKind::Expired);
            return false;
        }
        Bridged written = value.value;
        if (written.kind == BridgedKind::String) {
            written.string = StrDup(arena, written.string);
        }
        op.args[slot.site.argument] = written;
        return true;
    }
    if (slot.site.kind == SlotSiteKind::Handler &&
        value.kind == SlotValueKind::Handler) {
        if ((int)slot.site.op >= node->ops.len ||
            node->ops[slot.site.op].kind != SpecOpKind::Callback) {
            SetSpecError(error, SpecErrorKind::Expired);
            return false;
        }
        node->ops[slot.site.op].callback = value.handler;
        return true;
    }
    // Every pairing is decided when the slot is recorded, so a mismatch is the
    // runtime disagreeing with itself rather than a script mistake.
    SetSpecError(error, SpecErrorKind::Expired);
    return false;
}

bool SpecArena::ClaimVirtualItems(uint64_t count, uint64_t limit) {
    if (UINT64_MAX - virtualItems < count) return false;
    uint64_t total = virtualItems + count;
    if (total > limit) return false;
    virtualItems = total;
    return true;
}

static void Indent(Arena* a, StrBuilder* out, int depth) {
    for (int i = 0; i < depth; i++) StrBuilderAppend(a, *out, StrL("  "));
}

static void AppendBridged(Arena* a, StrBuilder* out, const Bridged& value) {
    switch (value.kind) {
        case BridgedKind::Nil:
            StrBuilderAppend(a, *out, StrL("nil"));
            break;
        case BridgedKind::Bool:
            StrBuilderAppend(a, *out,
                             value.boolean ? StrL("true") : StrL("false"));
            break;
        case BridgedKind::Number:
            StrBuilderAppend(a, *out, fmt("%g", value.number));
            break;
        case BridgedKind::String:
            StrBuilderAppendChar(a, *out, '"');
            StrBuilderAppend(a, *out, value.string);
            StrBuilderAppendChar(a, *out, '"');
            break;
    }
}

static void AppendArgs(Arena* a, StrBuilder* out, const SpecOp& op) {
    StrBuilderAppendChar(a, *out, '(');
    for (int i = 0; i < op.argCount; i++) {
        if (i) StrBuilderAppend(a, *out, StrL(", "));
        AppendBridged(a, out, op.args[i]);
    }
    StrBuilderAppendChar(a, *out, ')');
}

void SpecArena::WriteTree(Arena* a, StrBuilder* out, SpecId id,
                          int depth) const {
    const SpecNode* node = Node(id);
    if (!node) return;
    const Component& component = node->component;
    Indent(a, out, depth);
    StrBuilderAppend(a, *out, Str(ComponentName(component)));
    switch (component.kind) {
        case ComponentKind::Text:
        case ComponentKind::Button:
        case ComponentKind::Link:
        case ComponentKind::Checkbox:
        case ComponentKind::Switch:
        case ComponentKind::Scrollbar:
        case ComponentKind::Svg:
        case ComponentKind::Image:
        case ComponentKind::Accordion:
        case ComponentKind::AccordionTrigger:
        case ComponentKind::Pagination:
        // The path is what an avatar image *is*; without it the dump says an
        // image is there but not which one.
        case ComponentKind::AvatarImage:
        case ComponentKind::Tabs:
        case ComponentKind::Tab:
        case ComponentKind::Progress:
        case ComponentKind::Radio:
        case ComponentKind::Toggle:
        case ComponentKind::RadioGroup:
        case ComponentKind::ToggleGroup:
        case ComponentKind::Table:
        case ComponentKind::TableHeader:
        case ComponentKind::TableBody:
        case ComponentKind::TableCaption:
        case ComponentKind::Popover:
        case ComponentKind::HoverCard:
        case ComponentKind::Popup:
        case ComponentKind::Select:
        case ComponentKind::Combobox:
        case ComponentKind::HResizable:
        case ComponentKind::VResizable:
            StrBuilderAppend(a, *out, StrL(" \""));
            StrBuilderAppend(a, *out, component.text);
            StrBuilderAppendChar(a, *out, '"');
            break;
        case ComponentKind::TableRow:
        case ComponentKind::TableHead:
        case ComponentKind::TableCell:
            StrBuilderAppend(
                a, *out, fmt(" \"%s\" #%u", component.text, component.index));
            break;
        case ComponentKind::DatePicker:
            StrBuilderAppend(a, *out,
                             fmt(" \"%s\" #%llu", component.text,
                                 (unsigned long long)component.handle));
            break;
        case ComponentKind::ChildView:
        case ComponentKind::Input:
        case ComponentKind::Textarea:
        case ComponentKind::NumberInput:
        case ComponentKind::OtpInput:
        case ComponentKind::Slider:
        case ComponentKind::SliderTrack:
        case ComponentKind::SliderIndicator:
        case ComponentKind::SliderThumb:
            StrBuilderAppend(
                a, *out, fmt(" #%llu", (unsigned long long)component.handle));
            break;
        case ComponentKind::VVirtualList:
        case ComponentKind::HVirtualList:
            if (component.virtualList) {
                StrBuilderAppend(a, *out,
                                 fmt(" \"%s\" ×%d", component.virtualList->id,
                                     component.virtualList->sizeCount));
            }
            break;
        default:
            break;
    }

    for (const SpecOp& op : node->ops) {
        if (op.kind == SpecOpKind::NullaryStyle) {
            StrBuilderAppend(a, *out, StrL(" ."));
            StrBuilderAppend(a, *out, op.name);
        } else if (op.kind == SpecOpKind::ParamStyle) {
            StrBuilderAppend(a, *out, StrL(" ."));
            StrBuilderAppend(a, *out, op.name);
            AppendArgs(a, out, op);
        } else if (op.kind == SpecOpKind::Method) {
            StrBuilderAppend(a, *out, StrL(" :"));
            StrBuilderAppend(a, *out, op.name);
            AppendArgs(a, out, op);
        } else if (op.kind == SpecOpKind::Callback) {
            StrBuilderAppend(a, *out, StrL(" :"));
            StrBuilderAppend(a, *out, op.name);
            StrBuilderAppend(a, *out, StrL("(fn)"));
        } else if (op.kind == SpecOpKind::ActionCallback) {
            StrBuilderAppend(a, *out, StrL(" :on_action("));
            StrBuilderAppend(a, *out, op.name);
            StrBuilderAppend(a, *out, StrL(", fn)"));
        } else if (op.kind == SpecOpKind::StateStyle) {
            StrBuilderAppend(a, *out, StrL(" :"));
            StrBuilderAppend(a, *out, op.name);
            StrBuilderAppendChar(a, *out, '(');
            const SpecNode* state = Node(op.node);
            if (state) {
                for (const SpecOp& stateOp : state->ops) {
                    if (stateOp.kind == SpecOpKind::NullaryStyle ||
                        stateOp.kind == SpecOpKind::ParamStyle) {
                        StrBuilderAppend(a, *out, StrL("."));
                        StrBuilderAppend(a, *out, stateOp.name);
                        if (stateOp.kind == SpecOpKind::ParamStyle)
                            AppendArgs(a, out, stateOp);
                    }
                }
            } else {
                StrBuilderAppendChar(a, *out, '?');
            }
            StrBuilderAppendChar(a, *out, ')');
        }
    }
    StrBuilderAppendChar(a, *out, '\n');

    for (const SpecOp& op : node->ops) {
        if (op.kind != SpecOpKind::Slot) continue;
        Indent(a, out, depth + 1);
        StrBuilderAppendChar(a, *out, '@');
        StrBuilderAppend(a, *out, op.name);
        StrBuilderAppendChar(a, *out, '\n');
        WriteTree(a, out, op.node, depth + 2);
    }
    for (SpecId child : node->children) WriteTree(a, out, child, depth + 1);
}

Str SpecArena::DebugTree(Arena* into, SpecId root) const {
    StrBuilder out;
    WriteTree(into, &out, root, 0);
    return StrBuilderTakeStr(into, out);
}

} // namespace gpui::shell
