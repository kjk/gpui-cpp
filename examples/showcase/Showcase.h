/* GPUI Base showcase — C++ port of crates/base/examples/showcase. */

#pragma once

#include "gpui.h"

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
    int component = CompOverview;
    bool navigationEnabled = true;
    float scrollY = 0;

    bool accordionOpen[3] = {true, false, false};
    bool alertOpen = false;
    bool checkboxOn = true;
    bool collapsibleOpen = false;
    bool colorOpen = false;
    u32 colorHex = 0x2563eb;
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
    u8 toggleGroup = 0;
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

El* ScField(Arena* a, LineInput* in, int clickId, float w, bool valid);

const char* CompSlug(int i);
int CompFromSlug(const char* slug);
Str DupA(Arena* a, const char* s);
Str DupFmt(Arena* a, const char* fmt, ...);

El* ScTxt(Arena* a, Str s, float px, Rgba c);
El* ScBtnInk(Arena* a, int id, Str label);
El* ScBtnGhost(Arena* a, int id, Str label);
El* ScBtnLine(Arena* a, int id, Str label);
El* ScComingSoon(Arena* a, const char* name);

El* ShowcaseOverview(ShowcaseApp* app, Arena* a);
El* ShowcaseCalendarGrid(ShowcaseApp* app, Arena* a);
El* ShowcaseAccordion(ShowcaseApp* app, Arena* a);
El* ShowcaseAlertDialog(ShowcaseApp* app, Arena* a);
El* ShowcaseAvatar(ShowcaseApp* app, Arena* a);
El* ShowcaseButton(ShowcaseApp* app, Arena* a);
El* ShowcaseCalendar(ShowcaseApp* app, Arena* a);
El* ShowcaseCheckbox(ShowcaseApp* app, Arena* a);
El* ShowcaseCollapsible(ShowcaseApp* app, Arena* a);
El* ShowcaseColorPicker(ShowcaseApp* app, Arena* a);
El* ShowcaseCombobox(ShowcaseApp* app, Arena* a);
El* ShowcaseDatePicker(ShowcaseApp* app, Arena* a);
El* ShowcaseDialog(ShowcaseApp* app, Arena* a);
El* ShowcaseEditor(ShowcaseApp* app, Arena* a);
El* ShowcaseHoverCard(ShowcaseApp* app, Arena* a);
El* ShowcaseInput(ShowcaseApp* app, Arena* a);
El* ShowcaseLink(ShowcaseApp* app, Arena* a);
El* ShowcaseNumberInput(ShowcaseApp* app, Arena* a);
El* ShowcaseOtpInput(ShowcaseApp* app, Arena* a);
El* ShowcasePagination(ShowcaseApp* app, Arena* a);
El* ShowcasePopover(ShowcaseApp* app, Arena* a);
El* ShowcasePopup(ShowcaseApp* app, Arena* a);
El* ShowcaseProgress(ShowcaseApp* app, Arena* a);
El* ShowcaseRadio(ShowcaseApp* app, Arena* a);
El* ShowcaseRadioGroup(ShowcaseApp* app, Arena* a);
El* ShowcaseResizable(ShowcaseApp* app, Arena* a);
El* ShowcaseScrollbar(ShowcaseApp* app, Arena* a);
El* ShowcaseSelect(ShowcaseApp* app, Arena* a);
El* ShowcaseSheet(ShowcaseApp* app, Arena* a);
El* ShowcaseSlider(ShowcaseApp* app, Arena* a);
El* ShowcaseSwitch(ShowcaseApp* app, Arena* a);
El* ShowcaseTable(ShowcaseApp* app, Arena* a);
El* ShowcaseTabs(ShowcaseApp* app, Arena* a);
El* ShowcaseTextSelection(ShowcaseApp* app, Arena* a);
El* ShowcaseTextarea(ShowcaseApp* app, Arena* a);
El* ShowcaseToast(ShowcaseApp* app, Arena* a);
El* ShowcaseToggle(ShowcaseApp* app, Arena* a);
El* ShowcaseToggleGroup(ShowcaseApp* app, Arena* a);
El* ShowcaseTooltip(ShowcaseApp* app, Arena* a);
El* ShowcaseTree(ShowcaseApp* app, Arena* a);
El* ShowcaseVirtualList(ShowcaseApp* app, Arena* a);

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

void ShowcaseSliderDrag(ShowcaseApp* app, AppHost* host, float x, float y);
void ShowcaseResizeDrag(ShowcaseApp* app, AppHost* host, float x, float y);

typedef El* (*ShowcaseRenderFn)(ShowcaseApp* app, Arena* a, WinSize size);
typedef void (*ShowcaseClickFn)(ShowcaseApp* app, int id);
void ShowcaseRegister(int comp, ShowcaseRenderFn render, ShowcaseClickFn click);
El* ShowcaseRenderRegistered(ShowcaseApp* app, Arena* a, WinSize size);
void ShowcaseClickRegistered(ShowcaseApp* app, int id);

#define SHOWCASE_PAGE(COMP, RENDER, CLICK)                                    \
    namespace {                                                               \
    static El* _sc_render_##COMP(ShowcaseApp* app, Arena* a, WinSize size) {  \
        (void)size;                                                           \
        return RENDER(app, a);                                                \
    }                                                                         \
    struct _ScReg_##COMP {                                                    \
        _ScReg_##COMP() { ShowcaseRegister(COMP, _sc_render_##COMP, CLICK); } \
    } _sc_reg_##COMP;                                                         \
    }

#define SHOWCASE_PAGE_SZ(COMP, RENDER, CLICK)                                 \
    namespace {                                                               \
    static El* _sc_render_##COMP(ShowcaseApp* app, Arena* a, WinSize size) {  \
        return RENDER(app, a, size);                                          \
    }                                                                         \
    struct _ScReg_##COMP {                                                    \
        _ScReg_##COMP() { ShowcaseRegister(COMP, _sc_render_##COMP, CLICK); } \
    } _sc_reg_##COMP;                                                         \
    }

void ShowcaseClick(ShowcaseApp* app, AppHost* host, int id);
void ShowcaseChar(ShowcaseApp* app, AppHost* host, u32 cp);
void ShowcaseKey(ShowcaseApp* app, AppHost* host, int vk, bool down);
void ShowcaseWheel(ShowcaseApp* app, float x, float y, float delta);
void ShowcaseMouseMove(ShowcaseApp* app, AppHost* host, float x, float y);
void ShowcaseMouseDown(ShowcaseApp* app, AppHost* host, float x, float y,
                       int button);
void ShowcaseMouseUp(ShowcaseApp* app, AppHost* host, float x, float y,
                     int button);
