#ifndef GPUI_SHELL_SPEC_H_
#define GPUI_SHELL_SPEC_H_

#include "shell/value.h"

namespace gpui::shell {

using SpecId = uint32_t;
using CallbackId = uint64_t;

enum class BackgroundKind : uint8_t {
    Solid,
    LinearGradient,
    PatternSlash,
    Checkerboard,
};

struct BackgroundSpec {
    BackgroundKind kind = BackgroundKind::Solid;
    Str color;
    float opacity = 1;
    float angle = 0;
    Str fromColor;
    float fromPosition = 0;
    Str toColor;
    float toPosition = 1;
    Str colorSpace;
    float width = 0;
    float interval = 0;
    float size = 0;
};

enum class ComponentKind : uint8_t {
    Div,
    HFlex,
    VFlex,
    ChildView,
    Text,
    Button,
    Link,
    Checkbox,
    Switch,
    Scrollbar,
    Input,
    Textarea,
    NumberInput,
    OtpInput,
    Svg,
    Image,
    PathFill,
    PathStroke,
    Tabs,
    Tab,
    Progress,
    ProgressTrack,
    ProgressIndicator,
    FpsMonitor,
    Slider,
    SliderTrack,
    SliderIndicator,
    SliderThumb,
    Radio,
    Toggle,
    RadioGroup,
    ToggleGroup,
    Table,
    TableHeader,
    TableBody,
    TableRow,
    TableHead,
    TableCell,
    TableCaption,
    HResizable,
    VResizable,
    ResizablePanel,
    Collapsible,
    Popover,
    HoverCard,
    Popup,
    Select,
    Combobox,
    DatePicker,
    VVirtualList,
    HVirtualList,
};

struct VirtualListSpec {
    Str id;
    Axis axis = Axis::Vertical;
    Size* sizes = nullptr;
    int sizeCount = 0;
    CallbackId getKey = 0;
    CallbackId renderItems = 0;
};

struct Component {
    ComponentKind kind = ComponentKind::Div;
    Str text;
    uint64_t handle = 0;
    uint32_t index = 0;
    BackgroundSpec background;
    float strokeWidth = 0;
    VirtualListSpec* virtualList = nullptr;
};

const char* ComponentName(const Component& component);

enum class SpecOpKind : uint8_t {
    NullaryStyle,
    ParamStyle,
    Method,
    Callback,
    StateStyle,
    Slot,
};

struct SpecOp {
    SpecOpKind kind = SpecOpKind::Method;
    Str name;
    uint16_t styleIndex = 0;
    CallbackId callback = 0;
    SpecId node = 0;
    Bridged* args = nullptr;
    int argCount = 0;
};

struct SpecNode {
    Component component;
    ArenaVec<SpecOp> ops;
    ArenaVec<SpecId> children;
};

enum class SpecErrorKind : uint8_t {
    None,
    Claimed,
    Expired,
    AlreadyParented,
    SelfParent,
    DuplicateChildView,
};

struct SpecError {
    SpecErrorKind kind = SpecErrorKind::None;
    Str component;
};

Str SpecErrorMessage(Arena* arena, const SpecError& error);

class SpecArena {
  public:
    SpecArena();
    SpecArena(const SpecArena&) = delete;
    SpecArena& operator=(const SpecArena&) = delete;
    ~SpecArena();

    void Reset();
    int Len() const { return nodes.len; }
    bool IsEmpty() const { return nodes.len == 0; }
    SpecId Push(const Component& component);
    bool PushChildView(const Component& component, SpecId* out,
                       SpecError* error = nullptr);
    const SpecNode* Node(SpecId id) const;
    bool PushOp(SpecId id, const SpecOp& op, SpecError* error = nullptr);
    bool Claim(SpecId id, SpecError* error = nullptr);
    bool Attach(SpecId parent, SpecId child, SpecError* error = nullptr);
    bool ClaimVirtualItems(uint64_t count, uint64_t limit);
    Str DebugTree(Arena* into, SpecId root) const;

  private:
    Arena* arena = nullptr;
    Vec<SpecNode*> nodes;
    Vec<uint8_t> parented;
    Vec<uint8_t> claimed;
    Vec<uint64_t> mountedViews;
    uint64_t virtualItems = 0;

    bool CheckLive(SpecId id, SpecError* error) const;
    Component CopyComponent(const Component& component);
    SpecOp CopyOp(const SpecOp& op);
    void WriteTree(StrBuilder* out, SpecId id, int depth) const;
};

} // namespace gpui::shell
#endif // GPUI_SHELL_SPEC_H_
