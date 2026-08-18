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

enum {
    ClickBack = 1,
    ClickOverview = 100, // + Comp*
};

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
    bool comboboxOpen = false;
    char comboboxSel[32] = "Select framework";
    LineInput comboQuery = {};
    bool dateOpen = false;
    int calYear = 0; // 0 = fill from local date on first calendar render
    int calMonth = 0;
    int calDay = 0; // 0 = no selection; today is outlined like Rust
    bool dialogOpen = false;
    bool popoverOpen = false;
    bool popupOpen = false;
    int page = 3;
    float slider = 0.64f;
    LineInput input = {};
    LineInput hexIn = {};
    char textarea[2048] =
        "Build focused interfaces.\nKeep behavior composable.";
    int textareaLen = 0;
    bool textareaOn = false;
    char editor[4096] = {};
    int editorLen = 0;
    int editorCursor = 0;
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
    float resizeW = 124;
    bool draggingSlider = false;
    bool draggingResize = false;
    bool treeOpen[8] = {true, true, false, false, false, false, false, false};
    int treeSel = -1;
    int selA = -1;
    int selB = -1;
    int hoverId = 0;
};

El* ScField(Ctx* cx, LineInput* in, int clickId, float w, bool valid);

const char* CompSlug(int i);
int CompFromSlug(const char* slug);
Str DupA(Ctx* cx, const char* s);
Str DupFmt(Ctx* cx, const char* fmt, ...);

El* ScTxt(Ctx* cx, Str s, float px, Rgba c);
El* ScBtnInk(Ctx* cx, int id, Str label);
El* ScBtnGhost(Ctx* cx, int id, Str label);
El* ScBtnLine(Ctx* cx, int id, Str label);
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

void ShowcaseAccordionClick(ShowcaseApp* app, int id);
void ShowcaseAlertDialogClick(ShowcaseApp* app, int id);
void ShowcaseAvatarClick(ShowcaseApp* app, int id);
void ShowcaseButtonClick(ShowcaseApp* app, int id);
void ShowcaseCalendarClick(ShowcaseApp* app, int id);
void ShowcaseCheckboxClick(ShowcaseApp* app, int id);
void ShowcaseCollapsibleClick(ShowcaseApp* app, int id);
void ShowcaseColorPickerClick(ShowcaseApp* app, int id);
void ShowcaseComboboxClick(ShowcaseApp* app, int id);
void ShowcaseDatePickerClick(ShowcaseApp* app, int id);
void ShowcaseDialogClick(ShowcaseApp* app, int id);
void ShowcaseEditorClick(ShowcaseApp* app, int id);
void ShowcaseHoverCardClick(ShowcaseApp* app, int id);
void ShowcaseInputClick(ShowcaseApp* app, int id);
void ShowcaseLinkClick(ShowcaseApp* app, int id);
void ShowcaseNumberInputClick(ShowcaseApp* app, int id);
void ShowcaseOtpInputClick(ShowcaseApp* app, int id);
void ShowcasePaginationClick(ShowcaseApp* app, int id);
void ShowcasePopoverClick(ShowcaseApp* app, int id);
void ShowcasePopupClick(ShowcaseApp* app, int id);
void ShowcaseProgressClick(ShowcaseApp* app, int id);
void ShowcaseRadioClick(ShowcaseApp* app, int id);
void ShowcaseRadioGroupClick(ShowcaseApp* app, int id);
void ShowcaseResizableClick(ShowcaseApp* app, int id);
void ShowcaseScrollbarClick(ShowcaseApp* app, int id);
void ShowcaseSelectClick(ShowcaseApp* app, int id);
void ShowcaseSheetClick(ShowcaseApp* app, int id);
void ShowcaseSliderClick(ShowcaseApp* app, int id);
void ShowcaseSwitchClick(ShowcaseApp* app, int id);
void ShowcaseTableClick(ShowcaseApp* app, int id);
void ShowcaseTabsClick(ShowcaseApp* app, int id);
void ShowcaseTextSelectionClick(ShowcaseApp* app, int id);
void ShowcaseTextareaClick(ShowcaseApp* app, int id);
void ShowcaseToastClick(ShowcaseApp* app, int id);
void ShowcaseToggleClick(ShowcaseApp* app, int id);
void ShowcaseToggleGroupClick(ShowcaseApp* app, int id);
void ShowcaseTooltipClick(ShowcaseApp* app, int id);
void ShowcaseTreeClick(ShowcaseApp* app, int id);
void ShowcaseVirtualListClick(ShowcaseApp* app, int id);

void ShowcaseSliderDrag(ShowcaseApp* app, Window* host, float x, float y);
void ShowcaseResizeDrag(ShowcaseApp* app, Window* host, float x, float y);

typedef El* (*ShowcaseRenderFn)(ShowcaseApp* app, Ctx* cx, WinSize size);
typedef void (*ShowcaseClickFn)(ShowcaseApp* app, int id);
void ShowcaseRegister(int comp, ShowcaseRenderFn render, ShowcaseClickFn click);
El* ShowcaseRenderRegistered(ShowcaseApp* app, Ctx* cx, WinSize size);
void ShowcaseClickRegistered(ShowcaseApp* app, int id);

#define SHOWCASE_PAGE(COMP, RENDER, CLICK)                                    \
    namespace {                                                               \
    static El* _sc_render_##COMP(ShowcaseApp* app, Ctx* cx, WinSize size) {   \
        (void)size;                                                           \
        return RENDER(app, cx);                                               \
    }                                                                         \
    struct _ScReg_##COMP {                                                    \
        _ScReg_##COMP() { ShowcaseRegister(COMP, _sc_render_##COMP, CLICK); } \
    } _sc_reg_##COMP;                                                         \
    }

#define SHOWCASE_PAGE_SZ(COMP, RENDER, CLICK)                                 \
    namespace {                                                               \
    static El* _sc_render_##COMP(ShowcaseApp* app, Ctx* cx, WinSize size) {   \
        return RENDER(app, cx, size);                                         \
    }                                                                         \
    struct _ScReg_##COMP {                                                    \
        _ScReg_##COMP() { ShowcaseRegister(COMP, _sc_render_##COMP, CLICK); } \
    } _sc_reg_##COMP;                                                         \
    }

void ShowcaseClick(ShowcaseApp* app, Window* host, int id);
void ShowcaseChar(ShowcaseApp* app, Window* host, uint32_t cp);
void ShowcaseKey(ShowcaseApp* app, Window* host, int vk, bool down);
void ShowcaseWheel(ShowcaseApp* app, float x, float y, float delta);
void ShowcaseMouseMove(ShowcaseApp* app, Window* host, float x, float y);
void ShowcaseMouseDown(ShowcaseApp* app, Window* host, float x, float y,
                       int button);
void ShowcaseMouseUp(ShowcaseApp* app, Window* host, float x, float y,
                     int button);
