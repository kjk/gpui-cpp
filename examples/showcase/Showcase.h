/* GPUI Base showcase — C++ port of crates/base/examples/showcase. */

#include "gpui.h"

using namespace gpui;

enum {
    CompOverview = -1,
    CompAccordion = 0,
    CompAlertDialog,
    CompAvatar,
    CompButton,
    CompCalendar,
    CompCheckbox,
    CompCollapsible,
    CompColorPicker,
    CompCombobox,
    CompDatePicker,
    CompDialog,
    CompDock,
    CompEditor,
    CompHoverCard,
    CompInput,
    CompLink,
    CompNumberInput,
    CompOtpInput,
    CompPagination,
    CompPopover,
    CompPopup,
    CompProgress,
    CompRadio,
    CompRadioGroup,
    CompResizable,
    CompScrollbar,
    CompSelect,
    CompSheet,
    CompSlider,
    CompSwitch,
    CompTable,
    CompTabs,
    CompTextSelection,
    CompTextarea,
    CompToast,
    CompToggle,
    CompToggleGroup,
    CompTooltip,
    CompTree,
    CompVirtualList,
    CompCount,
};

// crates/base is the *unstyled* layer, so its showcase supplies the colors
// itself: the Rust pages write rgb(0x171717), rgb(0xd4d4d4) and friends
// inline. These mirror those literals; they are not theme tokens.
inline Rgba ScInk() {
    return Rgb(0x17, 0x17, 0x17);
}
inline Rgba ScWhite() {
    return Rgb(0xff, 0xff, 0xff);
}
inline Rgba ScMutedC() {
    return Rgb(0x73, 0x73, 0x73);
}
inline Rgba ScGray() {
    return Rgb(0x52, 0x52, 0x52);
}
inline Rgba ScBorder() {
    return Rgb(0xd4, 0xd4, 0xd4);
}
inline Rgba ScLine() {
    return Rgb(0xe5, 0xe5, 0xe5);
}
inline Rgba ScHover() {
    return Rgb(0xf5, 0xf5, 0xf5);
}
inline Rgba ScSilver() {
    return Rgb(0xa3, 0xa3, 0xa3);
}

// A button that reacts to the pointer. Hover, focus and hit-testing all key
// off an element identity, and the button now derives one from its own
// element id, the way Rust's `div().id(id)` does.
inline El* ScButton(Ctx* cx, Str id) {
    return Button::New(cx, id);
}

// The same, with the semantic states the caller declared. Rust spells it
// `Button::new(id).disabled(..).styles(|s| s.disabled(..))`.
inline El* ScButton(Ctx* cx, Str id, bool disabled, const ButtonStyles* styles,
                    bool selected = false) {
    return Button::New(cx, id, disabled, {}, true, styles, selected);
}

struct ShowcaseApp {
    static El* Render(ShowcaseApp* self, Ctx* cx);

    int component = CompOverview;
    bool navigationEnabled = true;
    float scrollY = 0;

    bool accordionOpen[3] = {true, false, false};
    bool alertOpen = false;
    bool checkboxOn = true;
    bool collapsibleOpen = false;
    bool colorOpen = false;
    uint32_t colorHex = 0x2563eb;
    // ColorPickerState's `preview: Option<Hsla>` — the color a swatch under
    // the pointer is showing, which no click has committed.
    uint32_t colorPreview = 0;
    bool colorHasPreview = false;
    bool comboboxOpen = false;
    char comboboxSel[32] = "Select framework";
    InputState comboQuery;
    bool dateOpen = false;
    // The calendar page's state, which is the calendar's own: the month it
    // is looking at, which grid is up and which page of the years — filled
    // from the local date on the first render.
    CalendarState cal = {};
    int calDay = 0; // 0 = no selection; today is outlined like Rust
    bool dialogOpen = false;
    bool popoverOpen = false;
    bool popupOpen = false;
    int page = 3;
    SliderState slider = {};
    InputState input;
    InputState hexIn;
    // Both multi-line pages hold the same engine as the single-line one, told
    // which kind it is: a TextareaState and an EditorState.
    InputState textarea;
    bool textareaOn = false;
    InputState editor;
    bool editorOn = false;
    bool editorInited = false;
    char otp[8] = "12";
    int otpLen = 2;
    bool otpOn = false;
    int radioSel = 0;
    bool switchOn = true;
    bool toggleOn = true;
    uint8_t toggleGroup = 0;
    int tab = 0;
    bool selectOpen = false;
    int selectIx = 0;
    bool sheetOpen = false;
    bool toastOn = false;
    float exampleScroll = 0;
    float virtualScroll = 0;
    // The tree page's state, which is the base tree's own: the items, what
    // is expanded, what is selected and where it is scrolled — `self.tree`
    // in the Rust page. Made on the page's first frame.
    Entity<TreeState> tree = {};
    // The dock page's state, which is the area's own: the tree of panels, the
    // three Docks and what a drag left them at — `self.dock` in the Rust
    // page, built once, since rebuilding it every frame would discard the
    // layout the viewer arranged.
    Entity<DockState> dock = {};
    int selA = -1;
    int selB = -1;
    // `self.tooltip_visible`, set by the trigger's on_hover. A page is told
    // what the pointer is doing; it does not ask the window.
    bool tooltipVisible = false;
};

const char* CompSlug(int i);
int CompFromSlug(const char* slug);
Str DupA(Ctx* cx, const char* s);
Str DupFmt(Ctx* cx, const char* fmt, ...);

El* ScTxt(Ctx* cx, Str s, float px, Rgba c);
El* ScBtnGhost(Ctx* cx, int id, Listener onClick, Str label);
El* ScComingSoon(Ctx* cx, const char* name);

El* ShowcaseOverview(ShowcaseApp* app, Ctx* cx);
El* ShowcaseCalendarGrid(ShowcaseApp* app, Ctx* cx);
El* ShowcaseAccordion(ShowcaseApp* app, Ctx* cx);
El* ShowcaseAlertDialog(ShowcaseApp* app, Ctx* cx);
El* ShowcaseAvatar(ShowcaseApp* app, Ctx* cx);
El* ShowcaseButton(ShowcaseApp* app, Ctx* cx);
El* ShowcaseCalendar(ShowcaseApp* app, Ctx* cx);
El* ShowcaseCheckbox(ShowcaseApp* app, Ctx* cx);
El* ShowcaseCollapsible(ShowcaseApp* app, Ctx* cx);
El* ShowcaseColorPicker(ShowcaseApp* app, Ctx* cx);
El* ShowcaseCombobox(ShowcaseApp* app, Ctx* cx);
El* ShowcaseDatePicker(ShowcaseApp* app, Ctx* cx);
El* ShowcaseDialog(ShowcaseApp* app, Ctx* cx);
El* ShowcaseDock(ShowcaseApp* app, Ctx* cx);
El* ShowcaseEditor(ShowcaseApp* app, Ctx* cx);
El* ShowcaseHoverCard(ShowcaseApp* app, Ctx* cx);
El* ShowcaseInput(ShowcaseApp* app, Ctx* cx);
El* ShowcaseLink(ShowcaseApp* app, Ctx* cx);
El* ShowcaseNumberInput(ShowcaseApp* app, Ctx* cx);
El* ShowcaseOtpInput(ShowcaseApp* app, Ctx* cx);
El* ShowcasePagination(ShowcaseApp* app, Ctx* cx);
El* ShowcasePopover(ShowcaseApp* app, Ctx* cx);
El* ShowcasePopup(ShowcaseApp* app, Ctx* cx);
El* ShowcaseProgress(ShowcaseApp* app, Ctx* cx);
El* ShowcaseRadio(ShowcaseApp* app, Ctx* cx);
El* ShowcaseRadioGroup(ShowcaseApp* app, Ctx* cx);
El* ShowcaseResizable(ShowcaseApp* app, Ctx* cx);
El* ShowcaseScrollbar(ShowcaseApp* app, Ctx* cx);
El* ShowcaseSelect(ShowcaseApp* app, Ctx* cx);
El* ShowcaseSheet(ShowcaseApp* app, Ctx* cx);
El* ShowcaseSlider(ShowcaseApp* app, Ctx* cx);
El* ShowcaseSwitch(ShowcaseApp* app, Ctx* cx);
El* ShowcaseTable(ShowcaseApp* app, Ctx* cx);
El* ShowcaseTabs(ShowcaseApp* app, Ctx* cx);
El* ShowcaseTextSelection(ShowcaseApp* app, Ctx* cx);
El* ShowcaseTextarea(ShowcaseApp* app, Ctx* cx);
El* ShowcaseToast(ShowcaseApp* app, Ctx* cx);
El* ShowcaseToggle(ShowcaseApp* app, Ctx* cx);
El* ShowcaseToggleGroup(ShowcaseApp* app, Ctx* cx);
El* ShowcaseTooltip(ShowcaseApp* app, Ctx* cx);
El* ShowcaseTree(ShowcaseApp* app, Ctx* cx);
El* ShowcaseVirtualList(ShowcaseApp* app, Ctx* cx);

typedef El* (*ShowcaseRenderFn)(ShowcaseApp* app, Ctx* cx, WinSize size);
void ShowcaseRegister(int comp, ShowcaseRenderFn render);
El* ShowcaseRenderRegistered(ShowcaseApp* app, Ctx* cx, WinSize size);

#define SHOWCASE_PAGE(COMP, RENDER)                                         \
    namespace {                                                             \
    static El* _sc_render_##COMP(ShowcaseApp* app, Ctx* cx, WinSize size) { \
        (void)size;                                                         \
        return RENDER(app, cx);                                             \
    }                                                                       \
    struct _ScReg_##COMP {                                                  \
        _ScReg_##COMP() { ShowcaseRegister(COMP, _sc_render_##COMP); }      \
    } _sc_reg_##COMP;                                                       \
    }

#define SHOWCASE_PAGE_SZ(COMP, RENDER)                                      \
    namespace {                                                             \
    static El* _sc_render_##COMP(ShowcaseApp* app, Ctx* cx, WinSize size) { \
        return RENDER(app, cx, size);                                       \
    }                                                                       \
    struct _ScReg_##COMP {                                                  \
        _ScReg_##COMP() { ShowcaseRegister(COMP, _sc_render_##COMP); }      \
    } _sc_reg_##COMP;                                                       \
    }

void ShowcaseChar(ShowcaseApp* app, Window* win, uint32_t cp);
void ShowcaseKey(ShowcaseApp* app, Window* win, int vk, bool down);
void ShowcaseWheel(ShowcaseApp* app, float x, float y, float delta);
void ShowcaseMouseMove(ShowcaseApp* app, Window* win, const MouseMoveEvent* ev);
void ShowcaseMouseDown(ShowcaseApp* app, Window* win, const MouseDownEvent* ev);
void ShowcaseMouseUp(ShowcaseApp* app, Window* win, const MouseUpEvent* ev);
