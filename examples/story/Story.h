/* C++ port of crates/story — GPUI Component gallery. */

#include "gpui.h"

using namespace gpui;

enum {
    StoryWelcome = 0,
    StoryAccordion,
    StoryAlert,
    StoryAlertDialog,
    StoryAvatar,
    StoryBadge,
    StoryBreadcrumb,
    StoryButton,
    StoryCalendar,
    StoryChart,
    StoryCheckbox,
    StoryClipboard,
    StoryCollapsible,
    StoryColorPicker,
    StoryCombobox,
    StoryDataTable,
    StoryDatePicker,
    StoryDescriptionList,
    StoryDialog,
    StoryDropdownButton,
    StoryEditor,
    StoryForm,
    StoryGroupBox,
    StoryHoverCard,
    StoryIcon,
    StoryImage,
    StoryInput,
    StoryKbd,
    StoryLabel,
    StoryList,
    StoryMenu,
    StoryNativeMenu,
    StoryNotification,
    StoryNumberInput,
    StoryOtpInput,
    StoryPagination,
    StoryPopover,
    StoryProgress,
    StoryRadio,
    StoryRating,
    StoryResizable,
    StoryScrollbar,
    StorySelect,
    StorySeparator,
    StorySettings,
    StorySheet,
    StorySidebar,
    StorySkeleton,
    StorySlider,
    StorySpinner,
    StoryStatusBar,
    StoryStepper,
    StorySwitch,
    StoryTable,
    StoryTabs,
    StoryTag,
    StoryTextarea,
    StoryThemeColors,
    StoryToggle,
    StoryTooltip,
    StoryTree,
    StoryVirtualList,
    StoryCount,
};

enum {
    ClickStory = 1000, // + Story*
    ClickSearch = 990,
    ClickCollapse = 991,
    ClickSearchClear = 992,
    ClickSizeXs = 980,
    ClickSizeSm = 981,
    ClickSizeMd = 982,
    ClickSizeLg = 983,
    ClickSizeMenu = 984,
    ClickOptsMenu = 985,
    ClickAccMultiple = 2120,
    ClickAccIcon = 2121,
    ClickAccDisabled = 2122,
    ClickAccBordered = 2123,
};

struct StoryApp {
    int story = StoryWelcome;
    float scrollY = 0;
    float sideScrollY = 0;
    bool collapsed = false;
    LineInput search = {};
    UiSize size = UiSize::Medium;
    bool sizeMenuOpen = false;
    bool accOptsOpen = false;

    bool accordionOpen[3] = {true, false, false};
    bool accordionStyledOpen[3] = {true, false, false};
    bool accordionMultiple = false;
    bool accordionIcon = false;
    bool accordionDisabled = false;
    bool accordionBordered = false;
    bool alertBanner = true;
    bool alertOpen = false;
    bool checkboxOn = true;
    bool checks[8] = {false, true};
    int crumbClicked = -1;
    bool switchOn = true;
    bool switches[8] = {true, false, true, true, false};
    bool toggleOn = false;
    int toggleSel = 1;
    bool toggles[10] = {};
    bool labelMasked = false;
    int radioSel = 0;
    int radioBilling = 1;
    int tab = 0;
    int page = 3;
    int rating = 3;
    int stepper = 1;
    LineInput field = {};
    char areaBuf[512] = "Build focused interfaces.";
    char otpBuf[8] = "12";
    int otpLen = 2;
    bool dialogOpen = false;
    bool sheetOpen = false;
    bool selectOpen = false;
    int selectIx = 0;
    int listSel = 0;
    int calYear = 2026;
    int calMonth = 8;
    int calDay = 17;
    bool notifyOn = false;
    bool tipRemoved = false;
    bool dateOpen = false;
    bool colorOpen = false;
    bool comboOpen = false;
    uint32_t colorHex = 0x2563eb;
    bool collapsibleOpen = false;
    bool collOpen[8] = {false, false, false, true, true, true, false, true};
    int pageMany = 12;
    int hoverId = 0;
    int selA = -1;
    int selB = -1;
    bool selecting = false;
};

struct StoryInfo {
    const char* slug;
    const char* title;
    const char* description;
};

const StoryInfo* StoryMeta(int i);
int StoryFromSlug(const char* slug);

Str StoryDup(Arena* a, const char* s);
Str StoryFmt(Arena* a, const char* f, ...);

El* StoryTxt(Arena* a, Str s, float px, Rgba c);
El* StorySection(Arena* a, const char* title, const char* desc);
El* StorySectionAdd(El* section, El* child);
El* StoryToolbar(Arena* a, StoryApp* app);
El* StoryToolbar(Arena* a, StoryApp* app, bool withOptions);
El* StoryComingSoon(Arena* a, int story);

typedef El* (*StoryRenderFn)(StoryApp* app, Arena* a, WinSize size);
typedef void (*StoryClickFn)(StoryApp* app, int id);
void StoryRegister(int story, StoryRenderFn render, StoryClickFn click);
El* StoryRenderRegistered(StoryApp* app, Arena* a, WinSize size);
void StoryClickRegistered(StoryApp* app, int id);

#define STORY_PAGE(ID, RENDER, CLICK)                                   \
    namespace {                                                         \
    static El* _st_render_##ID(StoryApp* app, Arena* a, WinSize size) { \
        (void)size;                                                     \
        return RENDER(app, a);                                          \
    }                                                                   \
    struct _StReg_##ID {                                                \
        _StReg_##ID() { StoryRegister(ID, _st_render_##ID, CLICK); }    \
    } _st_reg_##ID;                                                     \
    }

#define STORY_PAGE_SZ(ID, RENDER, CLICK)                                \
    namespace {                                                         \
    static El* _st_render_##ID(StoryApp* app, Arena* a, WinSize size) { \
        return RENDER(app, a, size);                                    \
    }                                                                   \
    struct _StReg_##ID {                                                \
        _StReg_##ID() { StoryRegister(ID, _st_render_##ID, CLICK); }    \
    } _st_reg_##ID;                                                     \
    }
