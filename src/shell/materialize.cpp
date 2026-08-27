#include "shell/materialize.h"

#include "base/button.h"
#include "base/checkbox.h"
#include "base/collapsible.h"
#include "base/combobox.h"
#include "base/date_picker.h"
#include "base/hover_card.h"
#include "base/input.h"
#include "base/link.h"
#include "base/number_input.h"
#include "base/otp_input.h"
#include "base/popover.h"
#include "base/popup.h"
#include "base/progress.h"
#include "base/radio.h"
#include "base/radio_group.h"
#include "base/resizable.h"
#include "base/scrollbar.h"
#include "base/select.h"
#include "base/slider.h"
#include "base/switch.h"
#include "base/table.h"
#include "base/tabs.h"
#include "base/toggle.h"
#include "base/toggle_group.h"
#include "fps/fps.h"
#include "shell/view.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace gpui {

struct MaterialBehavior {
    Str key;
    Str accessibilityLabel;
    Str href;
    bool disabled = false;
    bool selected = false;
    bool checked = false;
    bool pressed = false;
    bool indeterminate = false;
    bool open = false;
    bool hasOpen = false;
    bool defaultOpen = false;
    bool overlayClosable = true;
    bool controlsRight = false;
    bool start = false;
    Axis axis = Axis::Horizontal;
    float value = 0;
    int rowCount = -1;
    int columnCount = -1;
    int tabIndex = 0;
    bool tabStop = true;
    shell::CallbackId onClick = 0;
    shell::CallbackId onChange = 0;
    shell::CallbackId onHover = 0;
    shell::CallbackId onMouseMove = 0;
    shell::CallbackId onOpenChange = 0;
    shell::EntityHandle virtualScroll = 0;
};

static const shell::Bridged* Arg(const shell::SpecOp& op, int at) {
    return at >= 0 && at < op.argCount ? &op.args[at] : nullptr;
}

static bool AsBool(const shell::SpecOp& op, int at, bool fallback = false) {
    const shell::Bridged* value = Arg(op, at);
    return value ? shell::BridgedIsTruthy(*value) : fallback;
}

static float AsNumber(const shell::SpecOp& op, int at, float fallback = 0) {
    const shell::Bridged* value = Arg(op, at);
    if (!value || value->kind != shell::BridgedKind::Number ||
        !isfinite(value->number)) {
        return fallback;
    }
    return (float)value->number;
}

static Str AsString(const shell::SpecOp& op, int at) {
    const shell::Bridged* value = Arg(op, at);
    return value && value->kind == shell::BridgedKind::String ? value->string
                                                              : Str{};
}

static void ResolveBehavior(const shell::SpecNode* node,
                            MaterialBehavior* out) {
    for (const shell::SpecOp& op : node->ops) {
        if (op.kind == shell::SpecOpKind::Callback) {
            if (StrEq(op.name, "on_click")) out->onClick = op.callback;
            else if (StrEq(op.name, "on_change")) out->onChange = op.callback;
            else if (StrEq(op.name, "on_hover")) out->onHover = op.callback;
            else if (StrEq(op.name, "on_mouse_move")) out->onMouseMove = op.callback;
            else if (StrEq(op.name, "on_open_change")) out->onOpenChange = op.callback;
            continue;
        }
        if (op.kind != shell::SpecOpKind::Method) continue;
        if (StrEq(op.name, "id")) out->key = AsString(op, 0);
        else if (StrEq(op.name, "accessibility_label")) out->accessibilityLabel = AsString(op, 0);
        else if (StrEq(op.name, "href")) out->href = AsString(op, 0);
        else if (StrEq(op.name, "disabled")) out->disabled = AsBool(op, 0);
        else if (StrEq(op.name, "selected")) out->selected = AsBool(op, 0);
        else if (StrEq(op.name, "checked")) out->checked = AsBool(op, 0);
        else if (StrEq(op.name, "pressed")) out->pressed = AsBool(op, 0);
        else if (StrEq(op.name, "indeterminate")) out->indeterminate = AsBool(op, 0, true);
        else if (StrEq(op.name, "open")) {
            out->open = AsBool(op, 0);
            out->hasOpen = true;
        } else if (StrEq(op.name, "default_open")) out->defaultOpen = AsBool(op, 0);
        else if (StrEq(op.name, "overlay_closable")) out->overlayClosable = AsBool(op, 0);
        else if (StrEq(op.name, "controls_right")) out->controlsRight = AsBool(op, 0);
        else if (StrEq(op.name, "start")) out->start = AsBool(op, 0);
        else if (StrEq(op.name, "value")) out->value = AsNumber(op, 0);
        else if (StrEq(op.name, "row_count")) out->rowCount = (int)AsNumber(op, 0, -1);
        else if (StrEq(op.name, "column_count")) out->columnCount = (int)AsNumber(op, 0, -1);
        else if (StrEq(op.name, "tab_index")) out->tabIndex = (int)AsNumber(op, 0);
        else if (StrEq(op.name, "tab_stop")) out->tabStop = AsBool(op, 0, true);
        else if (StrEq(op.name, "track_scroll")) {
            const shell::Bridged* handle = Arg(op, 0);
            if (handle && handle->kind == shell::BridgedKind::Number &&
                handle->number >= 0)
                out->virtualScroll = (shell::EntityHandle)handle->number;
        }
        else if (StrEq(op.name, "axis")) {
            out->axis = StrEq(AsString(op, 0), "vertical") ? Axis::Vertical
                                                            : Axis::Horizontal;
        }
    }
}

static bool ParseNumber(Str text, float* out) {
    if (!text || text.len <= 0 || text.len >= 64) return false;
    char value[64] = {};
    memcpy(value, text.s, (size_t)text.len);
    char* end = nullptr;
    double number = strtod(value, &end);
    if (!end || end == value || !isfinite(number)) return false;
    while (*end == ' ' || *end == '\t') end++;
    if (*end) return false;
    *out = (float)number;
    return true;
}

struct MaterialLength {
    float pixels = 0;
    float fraction = 0;
    bool automatic = false;
    bool valid = false;
};

static Str TrimSpace(Str value) {
    while (value.len > 0 &&
           (value.s[0] == ' ' || value.s[0] == '\t' || value.s[0] == '\r' ||
            value.s[0] == '\n')) {
        value.s++;
        value.len--;
    }
    while (value.len > 0 &&
           (value.s[value.len - 1] == ' ' || value.s[value.len - 1] == '\t' ||
            value.s[value.len - 1] == '\r' || value.s[value.len - 1] == '\n')) {
        value.len--;
    }
    return value;
}

static MaterialLength LengthOf(const shell::Bridged* value) {
    MaterialLength result = {};
    if (!value) return result;
    if (value->kind == shell::BridgedKind::Number && isfinite(value->number)) {
        result.pixels = (float)value->number;
        result.valid = true;
        return result;
    }
    if (value->kind != shell::BridgedKind::String) return result;
    Str text = TrimSpace(value->string);
    if (StrEq(text, "auto")) {
        result.automatic = true;
        result.valid = true;
        return result;
    }
    float scale = 1;
    if (StrEndsWith(text, "%")) {
        text.len--;
        if (ParseNumber(text, &result.fraction)) {
            result.fraction /= 100.f;
            result.valid = true;
        }
        return result;
    }
    if (StrEndsWith(text, "rem")) {
        text.len -= 3;
        scale = 16;
    } else if (StrEndsWith(text, "px")) {
        text.len -= 2;
    }
    if (ParseNumber(text, &result.pixels)) {
        result.pixels *= scale;
        result.valid = true;
    }
    return result;
}

static bool StyleColor(const shell::SpecOp& op, Rgba* out) {
    const shell::Bridged* value = Arg(op, 0);
    Hsla color = {};
    if (!value || !shell::BridgedAsColor(*value, &color)) return false;
    *out = HslaToRgba(color);
    return true;
}

static float PresetNumber(Str name, Str prefix, bool* found) {
    *found = false;
    if (!StrStartsWith(name, prefix) || name.len <= prefix.len) return 0;
    Str suffix(name.s + prefix.len, name.len - prefix.len);
    char text[32] = {};
    if (suffix.len >= (int)sizeof(text)) return 0;
    for (int i = 0; i < suffix.len; i++) text[i] = suffix.s[i] == 'p' ? '.' : suffix.s[i];
    float value = 0;
    if (!ParseNumber(Str(text), &value)) return 0;
    *found = true;
    return value;
}

static bool ApplyNullary(El* element, Str name) {
    if (StrEq(name, "flex")) element->Flex();
    else if (StrEq(name, "flex_row")) element->FlexRow();
    else if (StrEq(name, "flex_col")) element->FlexCol();
    else if (StrEq(name, "flex_row_reverse")) element->FlexRowReverse();
    else if (StrEq(name, "flex_col_reverse")) element->FlexColReverse();
    else if (StrEq(name, "flex_wrap")) element->FlexWrap();
    else if (StrEq(name, "flex_1")) element->Flex1();
    else if (StrEq(name, "flex_none")) element->FlexNone();
    else if (StrEq(name, "grow")) element->Grow();
    else if (StrEq(name, "shrink_0")) element->Shrink0();
    else if (StrEq(name, "size_full")) element->SizeFull();
    else if (StrEq(name, "w_full")) element->W(kFill);
    else if (StrEq(name, "h_full")) element->H(kFill);
    else if (StrEq(name, "w_auto")) element->style.width = kAuto;
    else if (StrEq(name, "h_auto")) element->style.height = kAuto;
    else if (StrEq(name, "items_center")) element->ItemsCenter();
    else if (StrEq(name, "items_start")) element->ItemsStart();
    else if (StrEq(name, "items_end")) element->ItemsEnd();
    else if (StrEq(name, "items_stretch")) element->ItemsStretch();
    else if (StrEq(name, "justify_center")) element->JustifyCenter();
    else if (StrEq(name, "justify_start")) element->JustifyStart();
    else if (StrEq(name, "justify_end")) element->JustifyEnd();
    else if (StrEq(name, "justify_between")) element->JustifyBetween();
    else if (StrEq(name, "justify_around")) element->JustifyAround();
    else if (StrEq(name, "absolute")) element->Absolute();
    else if (StrEq(name, "fixed")) element->Fixed();
    else if (StrEq(name, "overflow_hidden")) element->ClipX()->ClipY();
    else if (StrEq(name, "overflow_x_hidden")) element->ClipX();
    else if (StrEq(name, "overflow_y_hidden")) element->ClipY();
    else if (StrEq(name, "overflow_scroll")) element->ScrollX(0)->ScrollY(0);
    else if (StrEq(name, "overflow_x_scroll")) element->ScrollX(0);
    else if (StrEq(name, "overflow_y_scroll")) element->ScrollY(0);
    else if (StrEq(name, "truncate")) element->Truncate();
    else if (StrEq(name, "whitespace_normal")) element->Wrap();
    else if (StrEq(name, "underline")) element->Underline();
    else if (StrEq(name, "italic")) element->Italic();
    else if (StrEq(name, "line_through")) element->Strikethrough();
    else if (StrEq(name, "font_medium")) element->Medium();
    else if (StrEq(name, "font_semibold")) element->Semibold();
    else if (StrEq(name, "font_bold")) element->Bold();
    else if (StrEq(name, "font_normal")) element->Weight(FontWeight::Normal);
    else if (StrEq(name, "font_thin")) element->Weight((FontWeight)100);
    else if (StrEq(name, "font_extralight")) element->Weight((FontWeight)200);
    else if (StrEq(name, "font_light")) element->Weight((FontWeight)300);
    else if (StrEq(name, "font_extrabold")) element->Weight((FontWeight)800);
    else if (StrEq(name, "font_black")) element->Weight((FontWeight)900);
    else if (StrEq(name, "text_xs")) element->Font(12);
    else if (StrEq(name, "text_sm")) element->Font(14);
    else if (StrEq(name, "text_base")) element->Font(16);
    else if (StrEq(name, "text_lg")) element->Font(18);
    else if (StrEq(name, "text_xl")) element->Font(20);
    else if (StrEq(name, "cursor_pointer")) element->Cursor(CursorKind::Pointer);
    else if (StrEq(name, "cursor_text")) element->Cursor(CursorKind::IBeam);
    else if (StrEq(name, "cursor_col_resize")) element->Cursor(CursorKind::ColResize);
    else if (StrEq(name, "cursor_row_resize")) element->Cursor(CursorKind::RowResize);
    else if (StrEq(name, "invisible")) element->Opacity(0);
    else if (StrEq(name, "visible")) element->Opacity(1);
    else {
        bool found = false;
        float n = PresetNumber(name, StrL("gap_"), &found);
        if (found) element->Gap(n * 4);
        else if ((n = PresetNumber(name, StrL("gap_x_"), &found)), found) element->GapX(n * 4);
        else if ((n = PresetNumber(name, StrL("gap_y_"), &found)), found) element->GapY(n * 4);
        else if ((n = PresetNumber(name, StrL("p_"), &found)), found) element->Pad(n * 4);
        else if ((n = PresetNumber(name, StrL("px_"), &found)), found) element->PadX(n * 4);
        else if ((n = PresetNumber(name, StrL("py_"), &found)), found) element->PadY(n * 4);
        else if ((n = PresetNumber(name, StrL("pt_"), &found)), found) element->PadT(n * 4);
        else if ((n = PresetNumber(name, StrL("pb_"), &found)), found) element->PadB(n * 4);
        else if ((n = PresetNumber(name, StrL("pl_"), &found)), found) element->PadL(n * 4);
        else if ((n = PresetNumber(name, StrL("pr_"), &found)), found) element->PadR(n * 4);
        else if ((n = PresetNumber(name, StrL("m_"), &found)), found) element->Margin(n * 4);
        else if ((n = PresetNumber(name, StrL("mx_"), &found)), found) element->MarginX(n * 4);
        else if ((n = PresetNumber(name, StrL("my_"), &found)), found) element->MarginY(n * 4);
        else if ((n = PresetNumber(name, StrL("mt_"), &found)), found) element->MarginT(n * 4);
        else if ((n = PresetNumber(name, StrL("mb_"), &found)), found) element->MarginB(n * 4);
        else if ((n = PresetNumber(name, StrL("ml_"), &found)), found) element->MarginL(n * 4);
        else if ((n = PresetNumber(name, StrL("mr_"), &found)), found) element->MarginR(n * 4);
        else if ((n = PresetNumber(name, StrL("rounded_"), &found)), found) element->Radius(n * 4);
        else if ((n = PresetNumber(name, StrL("border_"), &found)), found) element->style.border = n;
        else return false;
    }
    return true;
}

static bool ApplyParam(El* e, const shell::SpecOp& op) {
    MaterialLength length = LengthOf(Arg(op, 0));
    Rgba color = {};
    if (StrEq(op.name, "w") && length.valid) {
        if (length.fraction != 0) e->WFrac(length.fraction);
        else e->style.width = length.automatic ? kAuto : length.pixels;
    } else if (StrEq(op.name, "h") && length.valid) e->style.height = length.automatic ? kAuto : length.pixels;
    else if (StrEq(op.name, "size") && length.valid) {
        e->style.width = e->style.height = length.automatic ? kAuto : length.pixels;
    } else if (StrEq(op.name, "min_w") && length.valid) e->style.minW = length.automatic ? kAuto : length.pixels;
    else if (StrEq(op.name, "min_h") && length.valid) e->style.minH = length.automatic ? kAuto : length.pixels;
    else if (StrEq(op.name, "min_size") && length.valid) e->style.minW = e->style.minH = length.automatic ? kAuto : length.pixels;
    else if (StrEq(op.name, "max_w") && length.valid) e->style.maxW = length.automatic ? 1e9f : length.pixels;
    else if (StrEq(op.name, "max_h") && length.valid) e->style.maxH = length.automatic ? 1e9f : length.pixels;
    else if (StrEq(op.name, "max_size") && length.valid) e->style.maxW = e->style.maxH = length.automatic ? 1e9f : length.pixels;
    else if (StrEq(op.name, "p") && length.valid) e->Pad(length.pixels);
    else if (StrEq(op.name, "px") && length.valid) e->PadX(length.pixels);
    else if (StrEq(op.name, "py") && length.valid) e->PadY(length.pixels);
    else if (StrEq(op.name, "pt") && length.valid) e->PadT(length.pixels);
    else if (StrEq(op.name, "pb") && length.valid) e->PadB(length.pixels);
    else if (StrEq(op.name, "pl") && length.valid) e->PadL(length.pixels);
    else if (StrEq(op.name, "pr") && length.valid) e->PadR(length.pixels);
    else if (StrEq(op.name, "m") && length.valid) e->Margin(length.pixels);
    else if (StrEq(op.name, "mx") && length.valid) e->MarginX(length.pixels);
    else if (StrEq(op.name, "my") && length.valid) e->MarginY(length.pixels);
    else if (StrEq(op.name, "mt") && length.valid) e->MarginT(length.pixels);
    else if (StrEq(op.name, "mb") && length.valid) e->MarginB(length.pixels);
    else if (StrEq(op.name, "ml") && length.valid) e->MarginL(length.pixels);
    else if (StrEq(op.name, "mr") && length.valid) e->MarginR(length.pixels);
    else if (StrEq(op.name, "inset") && length.valid) e->Top(length.pixels)->Bottom(length.pixels)->Left(length.pixels)->Right(length.pixels);
    else if (StrEq(op.name, "top") && length.valid) {
        if (length.fraction != 0) e->TopRel(length.fraction); else e->Top(length.pixels);
    } else if (StrEq(op.name, "bottom") && length.valid) {
        if (length.fraction != 0) e->BottomRel(length.fraction); else e->Bottom(length.pixels);
    } else if (StrEq(op.name, "left") && length.valid) {
        if (length.fraction != 0) e->LeftRel(length.fraction); else e->Left(length.pixels);
    } else if (StrEq(op.name, "right") && length.valid) {
        if (length.fraction != 0) e->RightRel(length.fraction); else e->Right(length.pixels);
    }
    else if (StrEq(op.name, "gap") && length.valid) e->Gap(length.pixels);
    else if (StrEq(op.name, "gap_x") && length.valid) e->GapX(length.pixels);
    else if (StrEq(op.name, "gap_y") && length.valid) e->GapY(length.pixels);
    else if (StrEq(op.name, "flex_grow")) e->Grow(AsNumber(op, 0));
    else if (StrEq(op.name, "flex_shrink")) e->Shrink(AsNumber(op, 0));
    else if (StrEq(op.name, "flex_basis") && length.valid) e->Basis(length.pixels);
    else if (StrEq(op.name, "bg") && StyleColor(op, &color)) e->Bg(color);
    else if (StrEq(op.name, "text_color") && StyleColor(op, &color)) e->Fg(color);
    else if (StrEq(op.name, "text_size") && length.valid) e->Font(length.pixels);
    else if (StrEq(op.name, "font_family")) {
        if (StrEq(AsString(op, 0), "monospace")) e->Mono();
    } else if (StrEq(op.name, "font_weight")) e->Weight((FontWeight)(int)AsNumber(op, 0, 400));
    else if (StrEq(op.name, "line_height")) {
        const shell::Bridged* value = Arg(op, 0);
        if (value && value->kind == shell::BridgedKind::Number) e->LineHeight((float)value->number);
        else if (length.valid) e->LineHeight(length.pixels / (e->style.fontSize > 0 ? e->style.fontSize : 16));
    } else if (StrEq(op.name, "opacity")) e->Opacity(AsNumber(op, 0, 1));
    else if (StrEq(op.name, "border_color") && StyleColor(op, &color)) e->style.borderColor = color;
    else if (StrEq(op.name, "border") && length.valid) e->style.border = length.pixels;
    else if (StrEq(op.name, "border_t") && length.valid) e->style.borderT = length.pixels;
    else if (StrEq(op.name, "border_b") && length.valid) e->style.borderB = length.pixels;
    else if (StrEq(op.name, "border_l") && length.valid) e->style.borderL = length.pixels;
    else if (StrEq(op.name, "border_r") && length.valid) e->style.borderR = length.pixels;
    else if (StrEq(op.name, "border_x") && length.valid) e->style.borderL = e->style.borderR = length.pixels;
    else if (StrEq(op.name, "border_y") && length.valid) e->style.borderT = e->style.borderB = length.pixels;
    else if (StrEq(op.name, "rounded") && length.valid) e->Radius(length.pixels);
    else if (StrEq(op.name, "rounded_t") && length.valid) e->Corners(length.pixels, length.pixels, 0, 0);
    else if (StrEq(op.name, "rounded_b") && length.valid) e->Corners(0, 0, length.pixels, length.pixels);
    else if (StrEq(op.name, "rounded_l") && length.valid) e->Corners(length.pixels, 0, 0, length.pixels);
    else if (StrEq(op.name, "rounded_r") && length.valid) e->Corners(0, length.pixels, length.pixels, 0);
    else if (StrEq(op.name, "rounded_tl") && length.valid) e->Corners(length.pixels, 0, 0, 0);
    else if (StrEq(op.name, "rounded_tr") && length.valid) e->Corners(0, length.pixels, 0, 0);
    else if (StrEq(op.name, "rounded_br") && length.valid) e->Corners(0, 0, length.pixels, 0);
    else if (StrEq(op.name, "rounded_bl") && length.valid) e->Corners(0, 0, 0, length.pixels);
    else return false;
    return true;
}

static El* MaterializeNode(Ctx* cx, ShellRuntime* runtime,
                           const shell::SpecArena* specs, shell::SpecId id,
                           ShellError* error);

static El* Slot(Ctx* cx, ShellRuntime* runtime,
                const shell::SpecArena* specs, const shell::SpecNode* node,
                const char* name, ShellError* error) {
    for (const shell::SpecOp& op : node->ops) {
        if (op.kind == shell::SpecOpKind::Slot && StrEq(op.name, name)) {
            return MaterializeNode(cx, runtime, specs, op.node, error);
        }
    }
    return nullptr;
}

static Listener ClickListener(Ctx* cx, shell::CallbackId callback) {
    return callback ? Listen(cx, &ScriptView::OnClick, (intptr_t)callback)
                    : Listener{};
}

static Listener ChangeListener(Ctx* cx) {
    return Listen(cx, &ScriptView::OnChange);
}

struct MaterialVirtualUser {
    ShellRuntime* runtime = nullptr;
    shell::CallbackId render = 0;
    shell::CallbackId getKey = 0;
};

static void MaterialVirtualRange(void* user, Ctx* cx, int first, int end,
                                 El** out) {
    MaterialVirtualUser* values = (MaterialVirtualUser*)user;
    if (values && values->runtime) {
        values->runtime->RenderVirtualItems(values->render, values->getKey,
                                            first, end, cx, out);
    }
}

static El* Construct(Ctx* cx, ShellRuntime* runtime,
                     const shell::Component& component,
                     const MaterialBehavior& behavior) {
    Str id = component.text;
    Listener click = ClickListener(cx, behavior.onClick);
    switch (component.kind) {
        case shell::ComponentKind::Div: return Div(cx->a);
        case shell::ComponentKind::HFlex: return Div(cx->a)->FlexRow();
        case shell::ComponentKind::VFlex: return Div(cx->a)->FlexCol();
        case shell::ComponentKind::Text: return TextEl(cx->a, component.text);
        case shell::ComponentKind::Button:
            return Button::New(cx, id, behavior.disabled, click, true, nullptr,
                               behavior.selected);
        case shell::ComponentKind::Link:
            return Link::New(cx, id, behavior.disabled, click);
        case shell::ComponentKind::Checkbox: {
            CheckboxState state = behavior.indeterminate
                                      ? CheckboxState::Indeterminate
                                      : (behavior.checked ? CheckboxState::Checked
                                                          : CheckboxState::Unchecked);
            El* e = Checkbox::New(cx, id, state, behavior.disabled,
                                  behavior.onChange ? ChangeListener(cx) : Listener{},
                                  nullptr, nullptr, behavior.accessibilityLabel,
                                  behavior.tabIndex, behavior.tabStop);
            if (behavior.onChange && behavior.onChange <= INT32_MAX)
                e->Click((int)behavior.onChange);
            return e;
        }
        case shell::ComponentKind::Switch: {
            El* e = Switch::New(cx, id, behavior.checked, behavior.disabled,
                                behavior.onChange ? ChangeListener(cx) : Listener{},
                                nullptr, nullptr, behavior.accessibilityLabel,
                                behavior.tabIndex, behavior.tabStop);
            if (behavior.onChange && behavior.onChange <= INT32_MAX)
                e->Click((int)behavior.onChange);
            return e;
        }
        case shell::ComponentKind::Tabs: return Tabs::New(cx, id);
        case shell::ComponentKind::Tab:
            return Tab::New(cx, id, behavior.disabled, click, behavior.selected,
                            behavior.accessibilityLabel);
        case shell::ComponentKind::Progress:
            return Progress::New(cx, id, behavior.value, behavior.indeterminate,
                                 behavior.accessibilityLabel);
        case shell::ComponentKind::ProgressTrack: return ProgressTrack::New(cx);
        case shell::ComponentKind::ProgressIndicator: return ProgressIndicator::New(cx);
        case shell::ComponentKind::FpsMonitor: return FpsMonitorEl(cx);
        case shell::ComponentKind::Radio: {
            El* e = Radio::New(cx, id, behavior.checked, behavior.disabled,
                               behavior.onChange ? ChangeListener(cx) : Listener{});
            if (behavior.onChange && behavior.onChange <= INT32_MAX)
                e->Click((int)behavior.onChange);
            return e;
        }
        case shell::ComponentKind::Toggle: {
            El* e = Toggle::New(cx, id, behavior.pressed, behavior.disabled,
                                behavior.onChange ? ChangeListener(cx) : Listener{});
            if (behavior.onChange && behavior.onChange <= INT32_MAX)
                e->Click((int)behavior.onChange);
            return e;
        }
        case shell::ComponentKind::RadioGroup:
            return RadioGroup::New(cx, id, behavior.axis);
        case shell::ComponentKind::ToggleGroup:
            return ToggleGroup::New(cx, id, behavior.axis);
        case shell::ComponentKind::Table:
            return Table::New(cx, id, behavior.rowCount, behavior.columnCount,
                              behavior.accessibilityLabel);
        case shell::ComponentKind::TableHeader: return TableHeader::New(cx, id);
        case shell::ComponentKind::TableBody: return TableBody::New(cx, id);
        case shell::ComponentKind::TableRow: return TableRow::New(cx, id, (int)component.index);
        case shell::ComponentKind::TableHead: return TableHead::New(cx, id, (int)component.index);
        case shell::ComponentKind::TableCell: return TableCell::New(cx, id, (int)component.index);
        case shell::ComponentKind::TableCaption: return TableCaption::New(cx, id);
        case shell::ComponentKind::Select:
            return Select::New(cx, id, behavior.hasOpen ? behavior.open : behavior.defaultOpen,
                               behavior.disabled, behavior.accessibilityLabel);
        case shell::ComponentKind::Combobox: return Combobox::New(cx, id);
        case shell::ComponentKind::DatePicker:
            return DatePicker::New(cx, id, behavior.disabled,
                                   behavior.hasOpen ? behavior.open : behavior.defaultOpen);
        case shell::ComponentKind::Scrollbar: return Scrollbar::New(cx);
        case shell::ComponentKind::Svg: {
            El* icon = IconEl(cx->a, IconName::None);
            icon->iconPath = component.text;
            return icon;
        }
        case shell::ComponentKind::Image: return ImageEl(cx->a, component.text);
        case shell::ComponentKind::Slider:
        case shell::ComponentKind::SliderTrack:
        case shell::ComponentKind::SliderIndicator:
        case shell::ComponentKind::SliderThumb: {
            shell::RetainedEntry* retained =
                runtime ? runtime->Retained(component.handle) : nullptr;
            SliderState* state =
                retained && retained->kind == shell::RetainedKind::Slider
                    ? retained->slider
                    : nullptr;
            if (state && !behavior.disabled) {
                state->onChange = Listen(
                    cx, &ScriptView::OnSliderEvent,
                    (intptr_t)(uint32_t)retained->id);
            }
            if (component.kind == shell::ComponentKind::Slider)
                return Slider::New(cx, state, behavior.axis);
            if (component.kind == shell::ComponentKind::SliderTrack)
                return SliderTrack::New(cx, state, behavior.axis);
            if (component.kind == shell::ComponentKind::SliderIndicator)
                return SliderIndicator::New(cx, state);
            El* thumb = SliderThumb::New(cx);
            if (state) {
                float at = behavior.start ? state->pctLo : state->pctHi;
                thumb->Absolute();
                if (behavior.axis == Axis::Vertical)
                    thumb->BottomRel(at);
                else
                    thumb->LeftRel(at);
            }
            return thumb;
        }
        case shell::ComponentKind::Input:
        case shell::ComponentKind::Textarea: {
            shell::RetainedEntry* retained =
                runtime ? runtime->Retained(component.handle) : nullptr;
            bool textarea = component.kind == shell::ComponentKind::Textarea;
            InputState* state =
                retained &&
                        retained->kind == (textarea
                                              ? shell::RetainedKind::Textarea
                                              : shell::RetainedKind::Input)
                    ? retained->input
                    : nullptr;
            if (!state) return Div(cx->a);
            state->disabled = behavior.disabled;
            state->onChange = Listen(cx, &ScriptView::OnInputEvent,
                                     (intptr_t)(uint32_t)retained->id);
            Str nativeId = StrDup(
                cx->a, fmt("gpui-shell-%s-%u",
                           textarea ? StrL("textarea") : StrL("input"),
                           retained->id));
            El* frame = InputBase::New(
                            cx, nativeId, !behavior.disabled,
                            textarea ? AccessibilityRole::MultilineTextInput
                                     : AccessibilityRole::TextInput)
                            ->BindInput(behavior.disabled ? nullptr : state)
                            ->Flex()
                            ->W(kFill);
            if (textarea) {
                frame->ItemsStart()->Child(Textarea::New(cx, state));
            } else {
                frame->ItemsCenter()->Child(Input::New(cx, state));
            }
            return frame;
        }
        case shell::ComponentKind::NumberInput: {
            shell::RetainedEntry* retained =
                runtime ? runtime->Retained(component.handle) : nullptr;
            InputState* state =
                retained && retained->kind == shell::RetainedKind::Input
                    ? retained->input
                    : nullptr;
            if (!state) return Div(cx->a);
            state->disabled = behavior.disabled;
            state->onChange = Listen(cx, &ScriptView::OnInputEvent,
                                     (intptr_t)(uint32_t)retained->id);
            Str nativeId = StrDup(
                cx->a, fmt("gpui-shell-number-input-%u", retained->id));
            return NumberInput::New(cx, nativeId, state);
        }
        case shell::ComponentKind::OtpInput: {
            shell::RetainedEntry* retained =
                runtime ? runtime->Retained(component.handle) : nullptr;
            if (!retained || retained->kind != shell::RetainedKind::Otp)
                return Div(cx->a);
            OtpState* state = retained->otp.Get(cx);
            if (!state) return Div(cx->a);
            state->disabled = behavior.disabled;
            state->onChange = Listen(cx, &ScriptView::OnOtpEvent,
                                     (intptr_t)(uint32_t)retained->id);
            Str nativeId = StrDup(
                cx->a, fmt("gpui-shell-otp-%u", retained->id));
            return OtpInput::New(cx, nativeId, retained->otp);
        }
        case shell::ComponentKind::VVirtualList:
        case shell::ComponentKind::HVirtualList: {
            const shell::VirtualListSpec* list = component.virtualList;
            if (!list || !runtime) return Div(cx->a);
            float* sizes = list->sizeCount > 0
                               ? (float*)Alloc(
                                     cx->a,
                                     (int)(sizeof(float) *
                                           (size_t)list->sizeCount))
                               : nullptr;
            for (int i = 0; sizes && i < list->sizeCount; i++) {
                sizes[i] = list->axis == Axis::Horizontal ? list->sizes[i].w
                                                          : list->sizes[i].h;
            }
            MaterialVirtualUser* user = ArenaNew<MaterialVirtualUser>(cx->a);
            user->runtime = runtime;
            user->render = list->renderItems;
            user->getKey = list->getKey;
            VirtualListOpts opts;
            opts.count = list->sizeCount;
            opts.sizes = sizes;
            opts.layoutAxis = list->axis;
            opts.range = MaterialVirtualRange;
            opts.user = user;
            if (behavior.virtualScroll) {
                shell::RetainedEntry* scroll =
                    runtime->Retained(behavior.virtualScroll);
                if (scroll &&
                    scroll->kind == shell::RetainedKind::VirtualScroll)
                    opts.handle = &scroll->scroll;
            }
            return VirtualList::New(cx, list->id, opts);
        }
        case shell::ComponentKind::ChildView:
        case shell::ComponentKind::PathFill:
        case shell::ComponentKind::PathStroke:
            return Div(cx->a);
        case shell::ComponentKind::HResizable: return Div(cx->a)->FlexRow();
        case shell::ComponentKind::VResizable: return Div(cx->a)->FlexCol();
        case shell::ComponentKind::ResizablePanel: return Div(cx->a)->Flex1();
        case shell::ComponentKind::Collapsible:
        case shell::ComponentKind::Popover:
        case shell::ComponentKind::HoverCard:
        case shell::ComponentKind::Popup:
            return Div(cx->a);
    }
    return Div(cx->a);
}

static El* MaterializeNode(Ctx* cx, ShellRuntime* runtime,
                           const shell::SpecArena* specs, shell::SpecId id,
                           ShellError* error) {
    const shell::SpecNode* node = specs ? specs->Node(id) : nullptr;
    if (!node) return Div(cx->a);
    MaterialBehavior behavior = {};
    ResolveBehavior(node, &behavior);

    El* element = nullptr;
    if (node->component.kind == shell::ComponentKind::Collapsible) {
        Collapsible* collapsible = Collapsible::New(cx)->Open(behavior.open);
        for (shell::SpecId child : node->children)
            collapsible->Child(MaterializeNode(cx, runtime, specs, child, error));
        if (El* content = Slot(cx, runtime, specs, node, "content", error))
            collapsible->Content(content);
        element = collapsible->IntoEl();
    } else if (node->component.kind == shell::ComponentKind::Popup) {
        El* trigger = Slot(cx, runtime, specs, node, "trigger", error);
        El* content = Slot(cx, runtime, specs, node, "content", error);
        if (trigger) {
            Popup* popup = Popup::New(cx, node->component.text, trigger);
            if (content) popup->Content(content);
            element = popup->IntoEl();
        }
    } else if (node->component.kind == shell::ComponentKind::Popover) {
        El* trigger = Slot(cx, runtime, specs, node, "trigger", error);
        El* content = Slot(cx, runtime, specs, node, "content", error);
        if (trigger) {
            Popover* popover = Popover::New(cx, node->component.text)
                                    ->OverlayClosable(behavior.overlayClosable)
                                    ->Trigger(trigger);
            if (content) popover->Content(content);
            if (behavior.onOpenChange)
                popover->OnOpenChange(Listen(
                    cx, &ScriptView::OnOpenChange,
                    (intptr_t)behavior.onOpenChange));
            element = popover->IntoEl();
        }
    } else if (node->component.kind == shell::ComponentKind::HoverCard) {
        El* trigger = Slot(cx, runtime, specs, node, "trigger", error);
        El* content = Slot(cx, runtime, specs, node, "content", error);
        HoverCard* card = HoverCard::New(cx, node->component.text);
        if (trigger) card->Trigger(trigger);
        if (content && card->IsOpen()) card->Content(content);
        if (behavior.onOpenChange)
            card->OnOpenChange(Listen(cx, &ScriptView::OnOpenChange,
                                      (intptr_t)behavior.onOpenChange));
        element = card->IntoEl();
    }
    if (!element) element = Construct(cx, runtime, node->component, behavior);

    for (const shell::SpecOp& op : node->ops) {
        if (op.kind == shell::SpecOpKind::NullaryStyle) {
            if (!ApplyNullary(element, op.name) && error && !error->IsSet())
                ShellErrorSet(error, fmt("unknown style method `%s`", op.name));
        } else if (op.kind == shell::SpecOpKind::ParamStyle) {
            if (!ApplyParam(element, op) && error && !error->IsSet())
                ShellErrorSet(error, fmt("invalid style call `%s`", op.name));
        }
    }
    if (behavior.key) element->PathId(behavior.key);
    if (behavior.accessibilityLabel) element->AriaLabel(behavior.accessibilityLabel);
    element->TabIndex(behavior.tabIndex)->TabStop(behavior.tabStop);
    if (behavior.onClick && node->component.kind != shell::ComponentKind::Button &&
        node->component.kind != shell::ComponentKind::Link &&
        node->component.kind != shell::ComponentKind::Tab) {
        element->OnClick(ClickListener(cx, behavior.onClick));
    }
    if (behavior.onHover)
        element->OnHover(Listen(cx, &ScriptView::OnHover,
                                (intptr_t)behavior.onHover));
    if (behavior.onMouseMove)
        element->OnDragMove(Listen(cx, &ScriptView::OnMouseMove,
                                   (intptr_t)behavior.onMouseMove));
    if (node->component.kind != shell::ComponentKind::VVirtualList &&
        node->component.kind != shell::ComponentKind::HVirtualList) {
        for (shell::SpecId child : node->children) {
            element->Child(MaterializeNode(cx, runtime, specs, child, error));
        }
    }
    return element;
}

El* ShellMaterialize(Ctx* cx, ShellRuntime* runtime,
                     const RenderSnapshot* snapshot, ShellError* error) {
    if (!cx || !snapshot || !snapshot->Specs()) {
        ShellErrorSet(error, StrL("cannot materialize an empty shell snapshot"));
        return cx ? Div(cx->a) : nullptr;
    }
    double started = TimeNow();
    El* result = ShellMaterializeSpec(cx, runtime, snapshot->Specs(),
                                      snapshot->Root(), error);
    double elapsed = TimeNow() - started;
    if (runtime) {
        runtime->RecordMaterialize(
            elapsed <= 0 ? 0 : (uint64_t)(elapsed * 1e9));
    }
    return result;
}

El* ShellMaterializeSpec(Ctx* cx, ShellRuntime* runtime,
                         const shell::SpecArena* specs, shell::SpecId root,
                         ShellError* error) {
    if (!cx || !specs) return cx ? Div(cx->a) : nullptr;
    return MaterializeNode(cx, runtime, specs, root, error);
}

} // namespace gpui
