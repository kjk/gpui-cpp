#include "shell/materialize.h"

#include "base/button.h"
#include "base/actions.h"
#include "base/checkbox.h"
#include "base/collapsible.h"
#include "base/combobox.h"
#include "base/date_picker.h"
#include "base/hover_card.h"
#include "base/input.h"
#include "base/link.h"
#include "base/motion.h"
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
#include "base/state_style.h"
#include "base/switch.h"
#include "base/table.h"
#include "base/tabs.h"
#include "base/toggle.h"
#include "base/toggle_group.h"
#include "fps/fps.h"
#include "shell/a11y.h"
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
    bool scrollX = false;
    bool scrollY = false;
    bool scrollbar = false;
    bool hasScrollbarMode = false;
    ScrollbarMode scrollbarMode = ScrollbarMode::Scrolling;
    bool viewportFromLayout = false;
    Size scrollSize = {};
    bool hasScrollSize = false;
    bool panelVisible = true;
    bool hasPanelVisible = false;
    float panelSize = 0;
    bool hasPanelSize = false;
    float panelMin = kResizablePanelMinSize;
    float panelMax = 0;
    bool hasSizeRange = false;
    int positionInSet = 0;
    int sizeOfSet = 0;
    bool hasPosition = false;
    int itemToMeasure = 0;
    bool hasItemToMeasure = false;
    PopupAnchor anchor = PopupAnchor::TopLeft;
    bool hasAnchor = false;
    MouseButton mouseButton = MouseButton::Left;
    int openDelayMs = 600;
    int closeDelayMs = 300;
    shell::EntityHandle focus = 0;
    shell::EntityHandle contentFocus = 0;
    Str role;
    bool ariaSelected = false;
    bool hasAriaSelected = false;
    bool ariaActiveDescendant = false;
    Str tooltip;
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
    shell::CallbackId onConfirm = 0;
    shell::CallbackId onDismiss = 0;
    shell::CallbackId onStep = 0;
    shell::CallbackId onResize = 0;
    shell::CallbackId onItemClick = 0;
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

static shell::EntityHandle AsHandle(const shell::SpecOp& op, int at) {
    const shell::Bridged* value = Arg(op, at);
    if (!value || value->kind != shell::BridgedKind::Number ||
        value->number < 0 || !isfinite(value->number)) {
        return 0;
    }
    return (shell::EntityHandle)value->number;
}

static PopupAnchor AnchorOf(Str name, bool* found) {
    *found = true;
    if (StrEq(name, "top_left")) return PopupAnchor::TopLeft;
    if (StrEq(name, "top_right")) return PopupAnchor::TopRight;
    if (StrEq(name, "bottom_left")) return PopupAnchor::BottomLeft;
    if (StrEq(name, "bottom_right")) return PopupAnchor::BottomRight;
    if (StrEq(name, "top_center")) return PopupAnchor::TopCenter;
    if (StrEq(name, "bottom_center")) return PopupAnchor::BottomCenter;
    if (StrEq(name, "left_center")) return PopupAnchor::LeftCenter;
    if (StrEq(name, "right_center")) return PopupAnchor::RightCenter;
    *found = false;
    return PopupAnchor::TopLeft;
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
            else if (StrEq(op.name, "on_confirm")) out->onConfirm = op.callback;
            else if (StrEq(op.name, "on_dismiss")) out->onDismiss = op.callback;
            else if (StrEq(op.name, "on_step")) out->onStep = op.callback;
            else if (StrEq(op.name, "on_resize")) out->onResize = op.callback;
            else if (StrEq(op.name, "on_item_click")) out->onItemClick = op.callback;
            continue;
        }
        if (op.kind != shell::SpecOpKind::Method) continue;
        if (StrEq(op.name, "id")) out->key = AsString(op, 0);
        else if (StrEq(op.name, "accessibility_label")) out->accessibilityLabel = AsString(op, 0);
        else if (StrEq(op.name, "href")) out->href = AsString(op, 0);
        else if (StrEq(op.name, "disabled")) out->disabled = AsBool(op, 0, true);
        else if (StrEq(op.name, "selected")) out->selected = AsBool(op, 0, true);
        else if (StrEq(op.name, "checked")) out->checked = AsBool(op, 0, true);
        else if (StrEq(op.name, "pressed")) out->pressed = AsBool(op, 0, true);
        else if (StrEq(op.name, "indeterminate")) out->indeterminate = AsBool(op, 0, true);
        else if (StrEq(op.name, "open")) {
            out->open = AsBool(op, 0, true);
            out->hasOpen = true;
        } else if (StrEq(op.name, "default_open")) out->defaultOpen = AsBool(op, 0, true);
        else if (StrEq(op.name, "overlay_closable")) out->overlayClosable = AsBool(op, 0, true);
        else if (StrEq(op.name, "controls_right")) out->controlsRight = AsBool(op, 0, true);
        else if (StrEq(op.name, "start")) out->start = AsBool(op, 0, true);
        else if (StrEq(op.name, "value")) out->value = AsNumber(op, 0);
        else if (StrEq(op.name, "row_count")) out->rowCount = (int)AsNumber(op, 0, -1);
        else if (StrEq(op.name, "column_count")) out->columnCount = (int)AsNumber(op, 0, -1);
        else if (StrEq(op.name, "tab_index")) out->tabIndex = (int)AsNumber(op, 0);
        else if (StrEq(op.name, "tab_stop")) out->tabStop = AsBool(op, 0, true);
        else if (StrEq(op.name, "overflow_scroll")) out->scrollX = out->scrollY = true;
        else if (StrEq(op.name, "overflow_x_scroll")) out->scrollX = true;
        else if (StrEq(op.name, "overflow_y_scroll")) out->scrollY = true;
        else if (StrEq(op.name, "overflow_scrollbar")) {
            out->scrollX = out->scrollY = out->scrollbar = true;
        } else if (StrEq(op.name, "overflow_x_scrollbar")) {
            out->scrollX = out->scrollbar = true;
        } else if (StrEq(op.name, "overflow_y_scrollbar")) {
            out->scrollY = out->scrollbar = true;
        } else if (StrEq(op.name, "mode")) {
            Str mode = AsString(op, 0);
            out->hasScrollbarMode = true;
            if (StrEq(mode, "hover")) out->scrollbarMode = ScrollbarMode::Hover;
            else if (StrEq(mode, "always")) out->scrollbarMode = ScrollbarMode::Always;
            else out->scrollbarMode = ScrollbarMode::Scrolling;
        } else if (StrEq(op.name, "viewport_from_layout")) {
            out->viewportFromLayout = true;
        } else if (StrEq(op.name, "scroll_size")) {
            out->scrollSize = {AsNumber(op, 0), AsNumber(op, 1)};
            out->hasScrollSize = true;
        } else if (StrEq(op.name, "panel_visible")) {
            out->panelVisible = AsBool(op, 0, true);
            out->hasPanelVisible = true;
        } else if (StrEq(op.name, "panel_size")) {
            out->panelSize = AsNumber(op, 0);
            out->hasPanelSize = true;
        } else if (StrEq(op.name, "size_range")) {
            out->panelMin = AsNumber(op, 0, kResizablePanelMinSize);
            out->panelMax = AsNumber(op, 1, 0);
            out->hasSizeRange = true;
        } else if (StrEq(op.name, "set_position")) {
            out->positionInSet = (int)AsNumber(op, 0);
            out->sizeOfSet = (int)AsNumber(op, 1);
            out->hasPosition = true;
        } else if (StrEq(op.name, "anchor")) {
            out->anchor = AnchorOf(AsString(op, 0), &out->hasAnchor);
        } else if (StrEq(op.name, "mouse_button")) {
            Str button = AsString(op, 0);
            out->mouseButton = StrEq(button, "right")
                                   ? MouseButton::Right
                                   : (StrEq(button, "middle")
                                          ? MouseButton::Middle
                                          : MouseButton::Left);
        } else if (StrEq(op.name, "open_delay")) {
            out->openDelayMs = (int)AsNumber(op, 0, 600);
        } else if (StrEq(op.name, "close_delay")) {
            out->closeDelayMs = (int)AsNumber(op, 0, 300);
        } else if (StrEq(op.name, "track_focus")) {
            out->focus = AsHandle(op, 0);
        } else if (StrEq(op.name, "content_focus_handle")) {
            out->contentFocus = AsHandle(op, 0);
        } else if (StrEq(op.name, "role")) {
            out->role = AsString(op, 0);
        } else if (StrEq(op.name, "aria_selected")) {
            out->ariaSelected = AsBool(op, 0, true);
            out->hasAriaSelected = true;
        } else if (StrEq(op.name, "aria_active_descendant")) {
            out->ariaActiveDescendant = true;
        } else if (StrEq(op.name, "tooltip")) {
            out->tooltip = AsString(op, 0);
        }
        else if (StrEq(op.name, "track_scroll")) {
            const shell::Bridged* handle = Arg(op, 0);
            if (handle && handle->kind == shell::BridgedKind::Number &&
                handle->number >= 0)
                out->virtualScroll = (shell::EntityHandle)handle->number;
        }
        else if (StrEq(op.name, "axis")) {
            out->axis = StrEq(AsString(op, 0), "vertical") ? Axis::Vertical
                                                            : Axis::Horizontal;
        } else if (StrEq(op.name, "with_item_to_measure_index")) {
            out->itemToMeasure = (int)AsNumber(op, 0);
            out->hasItemToMeasure = true;
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

static uint32_t StyleFieldsFor(Str name) {
    if (StrEq(name, "bg")) return StyleFieldBg;
    if (StrEq(name, "text_color")) return StyleFieldColor;
    if (StrEq(name, "border_color")) return StyleFieldBorderColor;
    if (StrEq(name, "opacity") || StrEq(name, "invisible") ||
        StrEq(name, "visible"))
        return StyleFieldOpacity;
    if (StrEq(name, "w") || StrEq(name, "w_full") || StrEq(name, "w_auto"))
        return StyleFieldWidth;
    if (StrEq(name, "h") || StrEq(name, "h_full") || StrEq(name, "h_auto"))
        return StyleFieldHeight;
    if (StrEq(name, "size") || StrEq(name, "size_full"))
        return StyleFieldWidth | StyleFieldHeight;
    if (StrEq(name, "text_size") || StrStartsWith(name, "text_"))
        return StyleFieldFontSize;
    if (StrEq(name, "gap") || StrEq(name, "gap_x") ||
        StrEq(name, "gap_y") || StrStartsWith(name, "gap_"))
        return StyleFieldGap;
    if (StrEq(name, "p") || StrEq(name, "px") || StrEq(name, "py") ||
        StrEq(name, "pt") || StrEq(name, "pb") || StrEq(name, "pl") ||
        StrEq(name, "pr") || StrStartsWith(name, "p_"))
        return StyleFieldPad;
    if (StrEq(name, "m") || StrEq(name, "mx") || StrEq(name, "my") ||
        StrEq(name, "mt") || StrEq(name, "mb") || StrEq(name, "ml") ||
        StrEq(name, "mr") || StrStartsWith(name, "m_"))
        return StyleFieldMargin;
    if (StrStartsWith(name, "rounded")) return StyleFieldRadius;
    if (StrEq(name, "border_t")) return StyleFieldBorderT;
    if (StrEq(name, "border_b")) return StyleFieldBorderB;
    if (StrEq(name, "border_l")) return StyleFieldBorderL;
    if (StrEq(name, "border_r")) return StyleFieldBorderR;
    if (StrEq(name, "border_x"))
        return StyleFieldBorderL | StyleFieldBorderR;
    if (StrEq(name, "border_y"))
        return StyleFieldBorderT | StyleFieldBorderB;
    if (StrEq(name, "border") || StrStartsWith(name, "border_"))
        return StyleFieldBorder;
    return 0;
}

static bool ApplyStyleNode(Arena* arena, const shell::SpecNode* node,
                           El* target, uint32_t* fields,
                           ShellError* error) {
    if (!node || !target) return false;
    for (const shell::SpecOp& op : node->ops) {
        bool recognized = true;
        if (op.kind == shell::SpecOpKind::NullaryStyle) {
            recognized = ApplyNullary(target, op.name);
        } else if (op.kind == shell::SpecOpKind::ParamStyle) {
            recognized = ApplyParam(target, op);
        } else {
            continue;
        }
        if (!recognized) {
            if (error && !error->IsSet())
                ShellErrorSet(error,
                              fmt("invalid state style call `%s`", op.name));
            continue;
        }
        if (fields) *fields |= StyleFieldsFor(op.name);
    }
    (void)arena;
    return true;
}

static const shell::SpecNode* StateNode(const shell::SpecArena* specs,
                                        const shell::SpecNode* owner,
                                        const char* name) {
    if (!specs || !owner) return nullptr;
    for (const shell::SpecOp& op : owner->ops) {
        if (op.kind == shell::SpecOpKind::StateStyle &&
            StrEq(op.name, name))
            return specs->Node(op.node);
    }
    return nullptr;
}

static void ApplyStateNode(Ctx* cx, const shell::SpecNode* state,
                           El* target, const char* kind,
                           ShellError* error) {
    if (!state || !target) return;
    El* resolved = Div(cx->a);
    uint32_t fields = 0;
    ApplyStyleNode(cx->a, state, resolved, &fields, error);
    StateStyle style;
    style.style = resolved->style;
    style.set = fields;
    if (strcmp(kind, "hover") == 0) target->Hover(style);
    else if (strcmp(kind, "active") == 0) target->Active(style);
    else if (strcmp(kind, "focus") == 0) target->Focus(style);
}

static void ApplyStateStyles(Ctx* cx, const shell::SpecArena* specs,
                             const shell::SpecNode* owner, El* target,
                             ShellError* error) {
    ApplyStateNode(cx, StateNode(specs, owner, "hover"), target, "hover",
                   error);
    ApplyStateNode(cx, StateNode(specs, owner, "active"), target, "active",
                   error);
    ApplyStateNode(cx, StateNode(specs, owner, "focus"), target, "focus",
                   error);
}

struct MaterialPath {
    const shell::SpecNode* node = nullptr;
};

static bool PathCoordinate(const shell::Bridged* value, float origin,
                           float extent, float* out) {
    if (!value || !out) return false;
    if (value->kind == shell::BridgedKind::Number &&
        isfinite(value->number)) {
        *out = origin + (float)value->number;
        return true;
    }
    if (value->kind != shell::BridgedKind::String) return false;
    Str text = TrimSpace(value->string);
    if (!StrEndsWith(text, "%")) return false;
    text.len--;
    float percentage = 0;
    if (!ParseNumber(text, &percentage)) return false;
    *out = origin + extent * percentage / 100.f;
    return true;
}

static bool PathPoint(const shell::SpecOp& op, int at, Bounds bounds,
                      Point* out) {
    return PathCoordinate(Arg(op, at), bounds.x, bounds.w, &out->x) &&
           PathCoordinate(Arg(op, at + 1), bounds.y, bounds.h, &out->y);
}

static Point EllipsePoint(float cx, float cy, float rx, float ry,
                          float cosine, float sine, float angle) {
    float x = rx * cosf(angle), y = ry * sinf(angle);
    return {cx + cosine * x - sine * y,
            cy + sine * x + cosine * y};
}

static Point EllipseDerivative(float rx, float ry, float cosine,
                               float sine, float angle) {
    float x = -rx * sinf(angle), y = ry * cosf(angle);
    return {cosine * x - sine * y, sine * x + cosine * y};
}

static float VectorAngle(float ux, float uy, float vx, float vy) {
    float dot = ux * vx + uy * vy;
    float cross = ux * vy - uy * vx;
    return atan2f(cross, dot);
}

static void PathEllipticalArc(Path* path, Point from, Point to, float rx,
                              float ry, float rotationDegrees,
                              bool largeArc, bool sweep) {
    rx = fabsf(rx);
    ry = fabsf(ry);
    if (rx <= 0 || ry <= 0 ||
        (from.x == to.x && from.y == to.y)) {
        if (from.x != to.x || from.y != to.y)
            PathLineTo(path, to.x, to.y);
        return;
    }
    constexpr float kArcPi = 3.14159265358979323846f;
    float phi = rotationDegrees * kArcPi / 180.f;
    float cosine = cosf(phi), sine = sinf(phi);
    float dx = (from.x - to.x) * 0.5f;
    float dy = (from.y - to.y) * 0.5f;
    float x1 = cosine * dx + sine * dy;
    float y1 = -sine * dx + cosine * dy;
    float scale = x1 * x1 / (rx * rx) + y1 * y1 / (ry * ry);
    if (scale > 1.f) {
        float grow = sqrtf(scale);
        rx *= grow;
        ry *= grow;
    }
    float rx2 = rx * rx, ry2 = ry * ry;
    float denominator = rx2 * y1 * y1 + ry2 * x1 * x1;
    float numerator = rx2 * ry2 - denominator;
    float coefficient = denominator > 0
                            ? sqrtf(fmaxf(0.f, numerator / denominator))
                            : 0.f;
    if (largeArc == sweep) coefficient = -coefficient;
    float centerPrimeX = coefficient * rx * y1 / ry;
    float centerPrimeY = -coefficient * ry * x1 / rx;
    float centerX = cosine * centerPrimeX - sine * centerPrimeY +
                    (from.x + to.x) * 0.5f;
    float centerY = sine * centerPrimeX + cosine * centerPrimeY +
                    (from.y + to.y) * 0.5f;
    float ux = (x1 - centerPrimeX) / rx;
    float uy = (y1 - centerPrimeY) / ry;
    float vx = (-x1 - centerPrimeX) / rx;
    float vy = (-y1 - centerPrimeY) / ry;
    float start = atan2f(uy, ux);
    float delta = VectorAngle(ux, uy, vx, vy);
    if (!sweep && delta > 0) delta -= 2.f * kArcPi;
    if (sweep && delta < 0) delta += 2.f * kArcPi;
    int segments = (int)ceilf(fabsf(delta) / (kArcPi * 0.5f));
    if (segments < 1) segments = 1;
    float part = delta / (float)segments;
    float angle = start;
    for (int i = 0; i < segments; i++) {
        float next = angle + part;
        float alpha = 4.f / 3.f * tanf(part * 0.25f);
        Point p0 = EllipsePoint(centerX, centerY, rx, ry, cosine, sine,
                                angle);
        Point p1 = EllipsePoint(centerX, centerY, rx, ry, cosine, sine,
                                next);
        Point d0 = EllipseDerivative(rx, ry, cosine, sine, angle);
        Point d1 = EllipseDerivative(rx, ry, cosine, sine, next);
        PathCubicTo(path, p0.x + alpha * d0.x, p0.y + alpha * d0.y,
                    p1.x - alpha * d1.x, p1.y - alpha * d1.y, p1.x,
                    p1.y);
        angle = next;
    }
}

static bool MaterialPathBackground(const shell::BackgroundSpec& spec,
                                   Background* out) {
    Hsla first = {}, second = {};
    shell::Bridged value = shell::Bridged::String(
        spec.kind == shell::BackgroundKind::LinearGradient
            ? spec.fromColor
            : spec.color);
    if (!shell::BridgedAsColor(value, &first)) return false;
    Background result = HslaToRgba(first);
    if (spec.kind == shell::BackgroundKind::LinearGradient) {
        if (!shell::BridgedAsColor(shell::Bridged::String(spec.toColor),
                                   &second))
            return false;
        result = BackgroundLinear(
            spec.angle,
            ColorStopAt(HslaToRgba(first), spec.fromPosition),
            ColorStopAt(HslaToRgba(second), spec.toPosition));
    }
    // The renderer's Background currently has solid and linear-gradient
    // fills. PatternSlash and Checkerboard retain their geometry in the
    // snapshot for a future patterned path brush and use their declared
    // colour as the portable fallback today.
    *out = BackgroundOpacity(result, spec.opacity);
    return true;
}

static void PaintMaterialPath(PaintCtx* ctx, El* element, void* user) {
    MaterialPath* described = (MaterialPath*)user;
    if (!ctx || !element || !described || !described->node) return;
    const shell::SpecNode* node = described->node;
    Background background;
    if (!MaterialPathBackground(node->component.background, &background))
        return;
    Path* path = PathNew(ctx, true);
    if (!path) return;
    Bounds bounds = element->Bounds();
    Point current = {}, start = {};
    bool hasCurrent = false;
    for (const shell::SpecOp& op : node->ops) {
        if (StrEq(op.name, "move_to")) {
            Point point;
            if (!PathPoint(op, 0, bounds, &point)) continue;
            PathMoveTo(path, point.x, point.y);
            current = start = point;
            hasCurrent = true;
        } else if (StrEq(op.name, "line_to")) {
            Point point;
            if (!PathPoint(op, 0, bounds, &point)) continue;
            PathLineTo(path, point.x, point.y);
            current = point;
            if (!hasCurrent) start = point;
            hasCurrent = true;
        } else if (StrEq(op.name, "curve_to") && hasCurrent) {
            Point to, control;
            if (!PathPoint(op, 0, bounds, &to) ||
                !PathPoint(op, 2, bounds, &control))
                continue;
            Point a = {current.x + (control.x - current.x) * 2.f / 3.f,
                       current.y + (control.y - current.y) * 2.f / 3.f};
            Point b = {to.x + (control.x - to.x) * 2.f / 3.f,
                       to.y + (control.y - to.y) * 2.f / 3.f};
            PathCubicTo(path, a.x, a.y, b.x, b.y, to.x, to.y);
            current = to;
        } else if (StrEq(op.name, "cubic_bezier_to") && hasCurrent) {
            Point to, a, b;
            if (!PathPoint(op, 0, bounds, &to) ||
                !PathPoint(op, 2, bounds, &a) ||
                !PathPoint(op, 4, bounds, &b))
                continue;
            PathCubicTo(path, a.x, a.y, b.x, b.y, to.x, to.y);
            current = to;
        } else if (StrEq(op.name, "arc_to") && hasCurrent) {
            float rx = 0, ry = 0;
            Point to;
            if (!PathCoordinate(Arg(op, 0), 0, bounds.w, &rx) ||
                !PathCoordinate(Arg(op, 1), 0, bounds.h, &ry) ||
                !PathPoint(op, 5, bounds, &to))
                continue;
            PathEllipticalArc(path, current, to, rx, ry,
                              AsNumber(op, 2), AsBool(op, 3),
                              AsBool(op, 4));
            current = to;
        } else if (StrEq(op.name, "close") && hasCurrent) {
            PathClose(path);
            current = start;
        }
    }
    if (node->component.kind == shell::ComponentKind::PathFill) {
        if (background.gradient) {
            Point from, to;
            BackgroundLine(background, bounds, &from, &to);
            PathFillGradient(ctx, path, from.x, from.y, to.x, to.y,
                             background.from.color, background.to.color);
        } else {
            PathFill(ctx, path, background.color);
        }
    } else {
        PathStroke(ctx, path, node->component.strokeWidth,
                   background.color);
    }
    PathFree(path);
}

static const shell::SpecOp* MotionFor(const shell::SpecNode* node,
                                      const char* property) {
    const shell::SpecOp* found = nullptr;
    if (!node) return nullptr;
    for (const shell::SpecOp& op : node->ops) {
        if ((!StrEq(op.name, "transition") && !StrEq(op.name, "spring")) ||
            !StrEq(AsString(op, 0), property))
            continue;
        found = &op;
    }
    return found;
}

static bool MotionTarget(const shell::SpecNode* node, const char* property,
                         const Style& style, float* target) {
    bool declared = false;
    for (const shell::SpecOp& op : node->ops) {
        if ((strcmp(property, "opacity") == 0 && StrEq(op.name, "opacity")) ||
            (strcmp(property, "width") == 0 &&
             (StrEq(op.name, "w") || StrEq(op.name, "size"))) ||
            (strcmp(property, "height") == 0 &&
             (StrEq(op.name, "h") || StrEq(op.name, "size"))) ||
            (strcmp(property, "left") == 0 && StrEq(op.name, "left")) ||
            (strcmp(property, "top") == 0 && StrEq(op.name, "top")))
            declared = true;
    }
    if (!declared) return false;
    if (strcmp(property, "opacity") == 0) *target = style.opacity;
    else if (strcmp(property, "width") == 0) {
        if (style.width == kAuto || style.width == kFill ||
            style.widthFrac != 0)
            return false;
        *target = style.width;
    } else if (strcmp(property, "height") == 0) {
        if (style.height == kAuto || style.height == kFill) return false;
        *target = style.height;
    } else if (strcmp(property, "left") == 0) {
        if (style.absLeft == kAuto || style.absLeftRel != 0) return false;
        *target = style.absLeft;
    } else if (strcmp(property, "top") == 0) {
        if (style.absTop == kAuto || style.absTopRel != 0) return false;
        *target = style.absTop;
    } else {
        return false;
    }
    return true;
}

static Str MotionIdentity(Ctx* cx, const shell::SpecNode* node,
                          shell::SpecId specId,
                          const MaterialBehavior& behavior) {
    if (behavior.key) return behavior.key;
    const shell::Component& component = node->component;
    switch (component.kind) {
        case shell::ComponentKind::Button:
        case shell::ComponentKind::Link:
        case shell::ComponentKind::Checkbox:
        case shell::ComponentKind::Switch:
        case shell::ComponentKind::Tabs:
        case shell::ComponentKind::Tab:
        case shell::ComponentKind::Progress:
        case shell::ComponentKind::Radio:
        case shell::ComponentKind::Toggle:
        case shell::ComponentKind::RadioGroup:
        case shell::ComponentKind::ToggleGroup:
        case shell::ComponentKind::Table:
        case shell::ComponentKind::TableHeader:
        case shell::ComponentKind::TableBody:
        case shell::ComponentKind::TableRow:
        case shell::ComponentKind::TableHead:
        case shell::ComponentKind::TableCell:
        case shell::ComponentKind::TableCaption:
        case shell::ComponentKind::Scrollbar:
        case shell::ComponentKind::HResizable:
        case shell::ComponentKind::VResizable:
        case shell::ComponentKind::Popover:
        case shell::ComponentKind::HoverCard:
        case shell::ComponentKind::Popup:
        case shell::ComponentKind::Select:
        case shell::ComponentKind::Combobox:
        case shell::ComponentKind::DatePicker:
            if (component.text) return component.text;
            break;
        case shell::ComponentKind::VVirtualList:
        case shell::ComponentKind::HVirtualList:
            if (component.virtualList && component.virtualList->id)
                return component.virtualList->id;
            break;
        case shell::ComponentKind::Slider:
            return StrDup(cx->a,
                          fmt("gpui-shell-slider:%llu", component.handle));
        case shell::ComponentKind::SliderTrack:
            return StrDup(cx->a, fmt("gpui-shell-slider-track:%llu",
                                     component.handle));
        case shell::ComponentKind::SliderIndicator:
            return StrDup(cx->a, fmt("gpui-shell-slider-indicator:%llu",
                                     component.handle));
        case shell::ComponentKind::Input:
            return StrDup(cx->a,
                          fmt("gpui-shell-input:%llu", component.handle));
        case shell::ComponentKind::Textarea:
            return StrDup(cx->a, fmt("gpui-shell-textarea:%llu",
                                     component.handle));
        case shell::ComponentKind::NumberInput:
            return StrDup(cx->a, fmt("gpui-shell-number-input:%llu",
                                     component.handle));
        case shell::ComponentKind::OtpInput:
            return StrDup(cx->a,
                          fmt("gpui-shell-otp-input:%llu", component.handle));
        default: break;
    }
    return StrDup(cx->a, fmt("gpui-shell-spec-%u", specId));
}

static void ApplyMotions(Ctx* cx, const shell::SpecNode* node,
                         shell::SpecId specId,
                         const MaterialBehavior& behavior, El* element) {
    static const char* properties[] = {"opacity", "width", "height", "left",
                                       "top"};
    Str identity = MotionIdentity(cx, node, specId, behavior);
    for (const char* property : properties) {
        const shell::SpecOp* op = MotionFor(node, property);
        float target = 0;
        if (!op || !MotionTarget(node, property, element->style, &target))
            continue;
        uint32_t key = MotionId(identity, Str(property));
        float sampled = target;
        if (StrEq(op->name, "spring")) {
            Spring spring = SpringNew(AsNumber(*op, 1, 250));
            spring.damping = AsNumber(*op, 2, 1);
            spring.epsilon = AsNumber(*op, 3, 0.001f);
            sampled = SpringValue(cx, key, target, spring);
        } else {
            motion::Transition policy =
                motion::Transition::New(AsNumber(*op, 1));
            policy.delayMs = AsNumber(*op, 2);
            Str easing = AsString(*op, 3);
            if (StrEq(easing, "linear")) policy.ease = EaseLinear;
            else if (StrEq(easing, "ease-in")) policy.ease = EaseInCubic;
            else if (StrEq(easing, "ease-in-out"))
                policy.ease = EaseInOutCubic;
            sampled = MotionValue(cx, key, target, policy);
        }
        if (strcmp(property, "opacity") == 0) element->style.opacity = sampled;
        else if (strcmp(property, "width") == 0) element->style.width = sampled;
        else if (strcmp(property, "height") == 0) element->style.height = sampled;
        else if (strcmp(property, "left") == 0) element->style.absLeft = sampled;
        else element->style.absTop = sampled;
    }
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

static shell::SpecId SlotId(const shell::SpecNode* node, const char* name) {
    if (!node) return 0;
    for (const shell::SpecOp& op : node->ops) {
        if (op.kind == shell::SpecOpKind::Slot && StrEq(op.name, name))
            return op.node;
    }
    return 0;
}

static El* NumberStepButton(Ctx* cx, ShellRuntime* runtime,
                            const shell::SpecArena* specs,
                            shell::SpecId decoration, Str id, bool disabled,
                            Func0 step, ShellError* error) {
    El* button = Button::New(cx, id, disabled, {}, false);
    if (!disabled) button->OnClick(step);
    const shell::SpecNode* node =
        decoration && specs ? specs->Node(decoration) : nullptr;
    if (!node) return button;
    if (node->component.kind == shell::ComponentKind::HFlex)
        button->FlexRow();
    else if (node->component.kind == shell::ComponentKind::VFlex)
        button->FlexCol();
    uint32_t ignored = 0;
    ApplyStyleNode(cx->a, node, button, &ignored, error);
    ApplyStateStyles(cx, specs, node, button, error);
    MaterialBehavior behavior = {};
    ResolveBehavior(node, &behavior);
    if (behavior.accessibilityLabel)
        button->AriaLabel(behavior.accessibilityLabel);
    for (shell::SpecId child : node->children)
        button->Child(MaterializeNode(cx, runtime, specs, child, error));
    return button;
}

static Listener ClickListener(Ctx* cx, shell::CallbackId callback) {
    return callback ? Listen(cx, &ScriptView::OnClick, (intptr_t)callback)
                    : Listener{};
}

static Listener ChangeListener(Ctx* cx) {
    return Listen(cx, &ScriptView::OnChange);
}

struct MaterialOpenUrl {
    Str href;
};

static void MaterialOpenUrlRun(MaterialOpenUrl* call) {
    if (call && call->href) OpenUrl(call->href);
}

static Func0 OpenUrlCallback(Ctx* cx, Str href) {
    MaterialOpenUrl* call = ArenaNew<MaterialOpenUrl>(cx->a);
    call->href = href;
    return MkFunc0(&MaterialOpenUrlRun, call);
}

static FocusHandle RetainedFocus(ShellRuntime* runtime,
                                 shell::EntityHandle handle) {
    shell::RetainedEntry* entry =
        runtime && handle ? runtime->Retained(handle) : nullptr;
    return entry && entry->kind == shell::RetainedKind::Focus ? entry->focus
                                                              : FocusHandle{};
}

struct MaterialVirtualUser {
    ShellRuntime* runtime = nullptr;
    shell::CallbackId render = 0;
    shell::CallbackId getKey = 0;
    shell::CallbackId onItemClick = 0;
};

static void MaterialVirtualRange(void* user, Ctx* cx, int first, int end,
                                 El** out) {
    MaterialVirtualUser* values = (MaterialVirtualUser*)user;
    if (values && values->runtime) {
        values->runtime->RenderVirtualItems(
            values->render, values->getKey, values->onItemClick, first, end,
            cx, out);
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
        case shell::ComponentKind::Link: {
            El* link = Link::New(cx, id, behavior.disabled, click);
            if (!behavior.disabled && behavior.href)
                link->OnClick(OpenUrlCallback(cx, behavior.href));
            return link;
        }
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
        case shell::ComponentKind::Combobox: {
            bool open = behavior.hasOpen ? behavior.open : false;
            El* root = Select::New(cx, id, open, behavior.disabled,
                                   component.kind ==
                                           shell::ComponentKind::Select
                                       ? behavior.accessibilityLabel
                                       : Str{});
            ShellSelectBinding* binding =
                ArenaNew<ShellSelectBinding>(cx->a);
            binding->onOpenChange = behavior.onOpenChange;
            binding->onConfirm = behavior.onConfirm;
            binding->onDismiss = behavior.onDismiss;
            binding->open = open;
            binding->disabled = behavior.disabled;
            binding->triggerFocus = RetainedFocus(runtime, behavior.focus);
            binding->contentFocus =
                RetainedFocus(runtime, behavior.contentFocus);
            if (binding->triggerFocus.IsValid())
                root->TrackFocus(binding->triggerFocus);
            SelectInitKeys();
            Listener action = Listen(
                cx, &ScriptView::OnSelectAction, (intptr_t)binding);
            root->KeyContext(SelectContext())
                ->OnAction(action::SelectUp(), action)
                ->OnAction(action::SelectDown(), action)
                ->OnAction(action::Confirm(), action)
                ->OnAction(action::Cancel(), action);
            if (!open && !behavior.disabled && behavior.onOpenChange)
                root->OnAccessibilityDefault(Listen(
                    cx, &ScriptView::OnSelectOpen, (intptr_t)binding));
            return root;
        }
        case shell::ComponentKind::DatePicker: {
            bool open = behavior.hasOpen ? behavior.open : false;
            El* root = DatePicker::New(cx, id, behavior.disabled, open);
            FocusHandle focus = RetainedFocus(runtime, component.handle);
            if (!focus.IsValid()) return Div(cx->a);
            root->TrackFocus(focus)->TabStop(!behavior.disabled);
            if (behavior.onOpenChange) {
                ShellBoolBinding* toggle =
                    ArenaNew<ShellBoolBinding>(cx->a);
                toggle->callback = behavior.onOpenChange;
                toggle->value = !open;
                DatePickerBindKeys(
                    cx, root, id,
                    Listen(cx, &ScriptView::OnBoundBool,
                           (intptr_t)toggle),
                    Listener{}, open, behavior.disabled);
            }
            return root;
        }
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
                return Slider::New(cx, behavior.disabled ? nullptr : state,
                                   behavior.axis);
            if (component.kind == shell::ComponentKind::SliderTrack)
                return SliderTrack::New(cx,
                                        behavior.disabled ? nullptr : state,
                                        behavior.axis);
            if (component.kind == shell::ComponentKind::SliderIndicator)
                return SliderIndicator::New(cx, state);
            return SliderThumb::New(cx);
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
            user->onItemClick = behavior.onItemClick;
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
        case shell::ComponentKind::ChildView: {
            EntityId child = runtime
                                 ? runtime->NestedView(component.handle,
                                                       cx->app)
                                 : EntityId{};
            El* rendered = child.IsValid()
                               ? EntityRender(cx->app, cx->win, cx->a, child)
                               : nullptr;
            return rendered ? rendered : Div(cx->a);
        }
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
    bool childrenConsumed = false;
    if (node->component.kind == shell::ComponentKind::OtpInput) {
        shell::RetainedEntry* retained =
            runtime ? runtime->Retained(node->component.handle) : nullptr;
        if (!retained || retained->kind != shell::RetainedKind::Otp)
            return Div(cx->a);
        OtpState* state = retained->otp.Get(cx);
        if (!state) return Div(cx->a);
        state->disabled = behavior.disabled;
        state->onChange = Listen(cx, &ScriptView::OnOtpEvent,
                                 (intptr_t)(uint32_t)retained->id);
        Str nativeId =
            StrDup(cx->a, fmt("gpui-shell-otp-%u", retained->id));
        element = OtpInput::New(cx, nativeId, retained->otp);
        const shell::SpecNode* cellStyle =
            StateNode(specs, node, "cell_style");
        const shell::SpecNode* activeStyle =
            StateNode(specs, node, "cell_active_style");
        const shell::SpecNode* caretStyle =
            StateNode(specs, node, "caret_style");
        bool focused = !behavior.disabled &&
                       FocusHandleIsFocused(cx->win, state->focus);
        int active = focused
                         ? (state->len < state->length
                                ? state->len
                                : (state->length > 0 ? state->length - 1 : 0))
                         : -1;
        bool caret = focused && OtpCursorVisible(state, cx->app);
        for (int i = 0; i < state->length; i++) {
            El* cell = Div(cx->a);
            uint32_t ignored = 0;
            ApplyStyleNode(cx->a, cellStyle, cell, &ignored, error);
            if (i == active)
                ApplyStyleNode(cx->a, activeStyle, cell, &ignored, error);
            if (i < state->len) {
                cell->Child(TextEl(cx->a,
                                   state->masked ? StrL("•")
                                                 : Str(state->value + i, 1)));
            } else if (i == active && caret && caretStyle) {
                El* mark = Div(cx->a);
                ApplyStyleNode(cx->a, caretStyle, mark, &ignored, error);
                cell->Child(mark);
            }
            element->Child(cell);
        }
        for (shell::SpecId child : node->children)
            element->Child(
                MaterializeNode(cx, runtime, specs, child, error));
        childrenConsumed = true;
    } else if (node->component.kind == shell::ComponentKind::NumberInput) {
        shell::RetainedEntry* retained =
            runtime ? runtime->Retained(node->component.handle) : nullptr;
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
        Listener onStep =
            behavior.onStep
                ? Listen(cx, &ScriptView::OnNumberStep,
                         (intptr_t)behavior.onStep)
                : Listener{};
        NumberStep amount = NumberStep::Fixed(retained->number.step);
        const NumberStep* step =
            !behavior.onStep && retained->number.hasStep ? &amount : nullptr;
        Func0 decrement = NumberInputStepCallback(
            cx, state, StepAction::Decrement, step,
            retained->number.hasMin, retained->number.min,
            retained->number.hasMax, retained->number.max,
            behavior.disabled, onStep);
        Func0 increment = NumberInputStepCallback(
            cx, state, StepAction::Increment, step,
            retained->number.hasMin, retained->number.min,
            retained->number.hasMax, retained->number.max,
            behavior.disabled, onStep);
        El* decrementButton = NumberStepButton(
            cx, runtime, specs, SlotId(node, "decrement_button"),
            StrL("decrement"), behavior.disabled, decrement, error);
        El* incrementButton = NumberStepButton(
            cx, runtime, specs, SlotId(node, "increment_button"),
            StrL("increment"), behavior.disabled, increment, error);
        El* editor = Slot(cx, runtime, specs, node, "input", error);
        if (!editor) editor = Input::New(cx, state);
        El* ordinary = nullptr;
        if (node->children.len > 0) {
            ordinary = Div(cx->a)->FlexRow();
            for (shell::SpecId child : node->children)
                ordinary->Child(
                    MaterializeNode(cx, runtime, specs, child, error));
        }
        element = NumberInput::Compose(
            cx, nativeId, state, behavior.disabled, decrementButton, editor,
            incrementButton, behavior.controlsRight, ordinary);
        ShellNumberBinding* key = ArenaNew<ShellNumberBinding>(cx->a);
        key->state = state;
        key->step = amount;
        key->hasStep = retained->number.hasStep;
        key->hasMin = retained->number.hasMin;
        key->min = retained->number.min;
        key->hasMax = retained->number.hasMax;
        key->max = retained->number.max;
        key->disabled = behavior.disabled;
        key->onStep = behavior.onStep;
        element->TrackFocus(state->focus)
            ->OnAccessibilityDecrement(decrement)
            ->OnAccessibilityIncrement(increment)
            ->OnKeyDown(Listen(cx, &ScriptView::OnNumberKey,
                               (intptr_t)key));
        double numeric = 0;
        if (NumberParseValue(InputValue(state), &numeric))
            element->AriaNumericValue((float)numeric);
        childrenConsumed = true;
    } else if (node->component.kind == shell::ComponentKind::PathFill ||
               node->component.kind == shell::ComponentKind::PathStroke) {
        element = Div(cx->a);
        MaterialPath* path = ArenaNew<MaterialPath>(cx->a);
        path->node = node;
        element->customPaint = &PaintMaterialPath;
        element->customUser = path;
        childrenConsumed = true;
    } else if (node->component.kind == shell::ComponentKind::HResizable ||
        node->component.kind == shell::ComponentKind::VResizable) {
        Axis axis = node->component.kind == shell::ComponentKind::HResizable
                        ? Axis::Horizontal
                        : Axis::Vertical;
        ResizablePanelGroup* group =
            ResizablePanelGroup::New(cx, node->component.text, {}, axis);
        if (behavior.onResize) {
            group->OnResize(Listen(cx, &ScriptView::OnResize,
                                   (intptr_t)behavior.onResize));
        }
        for (shell::SpecId childId : node->children) {
            const shell::SpecNode* childNode = specs->Node(childId);
            El* content =
                MaterializeNode(cx, runtime, specs, childId, error);
            if (!childNode || childNode->component.kind !=
                                  shell::ComponentKind::ResizablePanel) {
                group->Grow(content);
                continue;
            }
            MaterialBehavior panelBehavior = {};
            ResolveBehavior(childNode, &panelBehavior);
            ResizablePanel* panel = ResizablePanel::New(cx)->Child(content);
            if (panelBehavior.hasPanelSize)
                panel->Size(panelBehavior.panelSize);
            if (panelBehavior.hasSizeRange)
                panel->SizeRange(panelBehavior.panelMin,
                                 panelBehavior.panelMax);
            if (panelBehavior.hasPanelVisible)
                panel->Visible(panelBehavior.panelVisible);
            if (content && content->style.flexGrow <= 0) panel->FlexNone();
            group->Child(panel);
        }
        element = group->IntoEl();
        childrenConsumed = true;
    } else if (node->component.kind == shell::ComponentKind::Collapsible) {
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
            Popup* popup = Popup::New(
                cx, node->component.text, trigger,
                behavior.hasAnchor ? behavior.anchor : PopupAnchor::TopLeft);
            if (content) popup->Content(content);
            element = popup->IntoEl();
        }
    } else if (node->component.kind == shell::ComponentKind::Popover) {
        El* trigger = Slot(cx, runtime, specs, node, "trigger", error);
        El* content = Slot(cx, runtime, specs, node, "content", error);
        if (trigger) {
            Entity<PopoverState> state = KeyedEntity<PopoverState>(
                cx, KeyedName(cx, node->component.text));
            PopoverState* held = state.Get(cx);
            if (held && !held->seeded) {
                held->seeded = true;
                held->open = behavior.defaultOpen;
            }
            if (behavior.hasOpen) PopoverSetOpen(cx, state, behavior.open);
            bool open = PopoverIsOpen(cx, state);
            Popover* popover = Popover::New(
                                    cx, node->component.text, state,
                                    behavior.mouseButton)
                                    ->OverlayClosable(behavior.overlayClosable)
                                    ->Trigger(trigger);
            if (behavior.hasAnchor) popover->Anchor(behavior.anchor);
            if (behavior.contentFocus && runtime) {
                shell::RetainedEntry* focus =
                    runtime->Retained(behavior.contentFocus);
                if (focus && focus->kind == shell::RetainedKind::Focus)
                    popover->TrackedFocus(focus->focus);
            }
            if (open && content) popover->Content(content);
            if (behavior.onOpenChange)
                popover->OnOpenChange(Listen(
                    cx, &ScriptView::OnOpenChange,
                    (intptr_t)behavior.onOpenChange));
            if (behavior.onDismiss)
                popover->OnDismiss(Listen(
                    cx, &ScriptView::OnClick,
                    (intptr_t)behavior.onDismiss));
            element = popover->IntoEl();
            if (open)
                CancelBindKeys(cx, element, "Popover", node->component.text,
                               ListenTo(state, &PopoverDismiss));
        }
    } else if (node->component.kind == shell::ComponentKind::HoverCard) {
        El* trigger = Slot(cx, runtime, specs, node, "trigger", error);
        El* content = Slot(cx, runtime, specs, node, "content", error);
        Entity<HoverCardState> state =
            HoverCardStateFor(cx, node->component.text);
        HoverCardSetDelays(cx, state, behavior.openDelayMs,
                           behavior.closeDelayMs);
        HoverCard* card =
            HoverCard::New(cx, node->component.text, state);
        if (behavior.onOpenChange)
            card->OnOpenChange(Listen(cx, &ScriptView::OnOpenChange,
                                      (intptr_t)behavior.onOpenChange));
        if (trigger) card->Trigger(trigger);
        if (content && card->IsOpen()) {
            PopupPlaceContent(
                content,
                behavior.hasAnchor ? behavior.anchor
                                   : PopupAnchor::TopCenter);
            card->Content(content);
        }
        element = card->IntoEl();
    }
    if (!element) element = Construct(cx, runtime, node->component, behavior);

    if (node->component.kind == shell::ComponentKind::SliderIndicator) {
        shell::RetainedEntry* retained =
            runtime ? runtime->Retained(node->component.handle) : nullptr;
        SliderState* state =
            retained && retained->kind == shell::RetainedKind::Slider
                ? retained->slider
                : nullptr;
        const shell::SpecNode* range =
            StateNode(specs, node, "range_style");
        if (state && range) {
            El* fill = Div(cx->a)->Absolute();
            uint32_t ignored = 0;
            ApplyStyleNode(cx->a, range, fill, &ignored, error);
            if (behavior.axis == Axis::Vertical) {
                fill->BottomRel(state->pctLo)
                    ->TopRel(1.f - state->pctHi)
                    ->W(kFill);
            } else {
                fill->LeftRel(state->pctLo)
                    ->RightRel(1.f - state->pctHi)
                    ->H(kFill);
            }
            element->Child(fill);
        }
    }

    for (const shell::SpecOp& op : node->ops) {
        if (op.kind == shell::SpecOpKind::NullaryStyle) {
            if (!ApplyNullary(element, op.name) && error && !error->IsSet())
                ShellErrorSet(error, fmt("unknown style method `%s`", op.name));
        } else if (op.kind == shell::SpecOpKind::ParamStyle) {
            if (!ApplyParam(element, op) && error && !error->IsSet())
                ShellErrorSet(error, fmt("invalid style call `%s`", op.name));
        }
    }
    ApplyMotions(cx, node, id, behavior, element);
    if (node->component.kind == shell::ComponentKind::SliderThumb) {
        shell::RetainedEntry* retained =
            runtime ? runtime->Retained(node->component.handle) : nullptr;
        SliderState* state =
            retained && retained->kind == shell::RetainedKind::Slider
                ? retained->slider
                : nullptr;
        if (state) {
            float at = behavior.start ? state->pctLo : state->pctHi;
            element->Absolute();
            if (behavior.axis == Axis::Vertical) {
                element->style.absTop = kAuto;
                element->style.absTopRel = 0;
                element->BottomRel(at);
            } else {
                element->style.absRight = kAuto;
                element->style.absRightRel = 0;
                element->LeftRel(at);
            }
        }
    }
    ApplyStateStyles(cx, specs, node, element, error);
    if (behavior.key) element->PathId(behavior.key);
    if (behavior.accessibilityLabel) element->AriaLabel(behavior.accessibilityLabel);
    if (behavior.role) {
        AccessibilityRole role = shell::AccessibilityRoleFromName(behavior.role);
        if (role != AccessibilityRole::None) element->Role(role);
    }
    if (behavior.hasAriaSelected)
        element->AriaSelected(behavior.ariaSelected);
    if (behavior.ariaActiveDescendant) element->AriaActiveDescendant();
    if (behavior.hasPosition) {
        element->AriaPositionInSet(behavior.positionInSet)
            ->AriaSizeOfSet(behavior.sizeOfSet);
    }
    if (behavior.focus && runtime) {
        shell::RetainedEntry* focus = runtime->Retained(behavior.focus);
        if (focus && focus->kind == shell::RetainedKind::Focus)
            element->TrackFocus(focus->focus);
    }
    if (behavior.tooltip) element->Tip(behavior.tooltip);
    element->TabIndex(behavior.tabIndex)->TabStop(behavior.tabStop);
    if (behavior.scrollX || behavior.scrollY ||
        node->component.kind == shell::ComponentKind::Scrollbar) {
        if (behavior.scrollX ||
            node->component.kind == shell::ComponentKind::Scrollbar)
            element->ScrollX(0);
        if (behavior.scrollY ||
            node->component.kind == shell::ComponentKind::Scrollbar)
            element->ScrollY(0);
        Str scrollName = behavior.key ? behavior.key : node->component.text;
        if (scrollName) element->ScrollId(HashClickId(scrollName));
        else element->ScrollFromPath();
        if (!behavior.scrollbar &&
            node->component.kind != shell::ComponentKind::Scrollbar) {
            if (behavior.scrollX) element->HideScrollbarX();
            if (behavior.scrollY) element->HideScrollbarY();
        }
        if (behavior.hasScrollbarMode)
            element->ScrollMode(behavior.scrollbarMode);
    }
    if (!behavior.disabled && behavior.onClick &&
        node->component.kind != shell::ComponentKind::Button &&
        node->component.kind != shell::ComponentKind::Link &&
        node->component.kind != shell::ComponentKind::Tab) {
        element->OnClick(ClickListener(cx, behavior.onClick));
    }
    if (behavior.onHover)
        element->OnHover(Listen(cx, &ScriptView::OnHover,
                                (intptr_t)behavior.onHover));
    if (behavior.onMouseMove)
        element->OnMouseMove(Listen(cx, &ScriptView::OnMouseMove,
                                    (intptr_t)behavior.onMouseMove));
    if (!childrenConsumed &&
        node->component.kind != shell::ComponentKind::VVirtualList &&
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
