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
    // Esc reaches the active page so it can close its overlays.
    ClickEscape = -100,
};

struct StoryApp {
    static El* Render(StoryApp* self, Ctx* cx);

    int story = StoryWelcome;
    float scrollY = 0;
    float sideScrollY = 0;
    bool collapsed = false;
    LineInput search = {};
    int selA = -1;
    int selB = -1;
    bool selecting = false;
    // One entity per story, created on first view. crates/story keeps the
    // same shape: Gallery holds a view per story, not their state.
    EntityId pages[StoryCount] = {};
};

struct StoryInfo {
    const char* slug;
    const char* title;
    const char* description;
};

const StoryInfo* StoryMeta(int i);
int StoryFromSlug(const char* slug);

Str StoryDup(Ctx* cx, const char* s);
Str StoryFmt(Ctx* cx, const char* f, ...);

El* StoryTxt(Ctx* cx, Str s, float px, Rgba c);
El* StorySection(Ctx* cx, const char* title, const char* desc);
El* StorySectionAdd(El* section, El* child);
El* StoryComingSoon(Ctx* cx, int story);

// story_toolbar(size): the Size dropdown, plus an Options dropdown for the
// pages that have one. Each page owns its copy.
struct StoryToolbarState {
    UiSize size = UiSize::Medium;
    bool sizeMenuOpen = false;
    bool optsOpen = false;
};

// accordion_story builds the Options dropdown; it is the only page with one.
struct StoryAccordionOptions {
    bool multiple = false;
    bool icon = false;
    bool disabled = false;
    bool bordered = false;
};

// What a toolbar row does, bound into its listener the way a Rust closure
// would capture it.
enum StoryToolbarAction {
    ToolbarOpenSize = 1,
    ToolbarOpenOpts,
    ToolbarSizeXs,
    ToolbarSizeSm,
    ToolbarSizeMd,
    ToolbarSizeLg,
    ToolbarOptMultiple,
    ToolbarOptIcon,
    ToolbarOptDisabled,
    ToolbarOptBordered,
};

void StoryToolbarApply(StoryToolbarState* st, StoryAccordionOptions* opts,
                       int act);
El* StoryToolbarCore(Ctx* cx, StoryToolbarState* st,
                     StoryAccordionOptions* opts, Listener onAct);

template <typename T>
void StoryToolbarAct(T* self, Ctx* cx, const ClickEvent*, intptr_t act) {
    StoryToolbarApply(&self->toolbar, nullptr, (int)act);
    Notify(cx);
}

template <typename T>
void StoryToolbarActOpts(T* self, Ctx* cx, const ClickEvent*, intptr_t act) {
    StoryToolbarApply(&self->toolbar, &self->options, (int)act);
    Notify(cx);
}

// story_toolbar(self.size): the page owns the state, the rows own their
// handlers.
template <typename T>
El* StoryToolbar(Ctx* cx, T* self) {
    return StoryToolbarCore(cx, &self->toolbar, nullptr,
                            Listen(cx, &StoryToolbarAct<T>));
}

template <typename T>
El* StoryToolbarWithOptions(Ctx* cx, T* self) {
    return StoryToolbarCore(cx, &self->toolbar, &self->options,
                            Listen(cx, &StoryToolbarActOpts<T>));
}

typedef EntityId (*StoryPageNewFn)(App* app);
typedef void (*StoryPageClickFn)(void* self, Ctx* cx, int id);

void StoryRegister(int story, StoryPageNewFn create, StoryPageClickFn click);
El* StoryRenderRegistered(StoryApp* app, Ctx* cx);
void StoryClickRegistered(StoryApp* app, Ctx* cx, int id);

#define STORY_PAGE(ID, TYPE)                                               \
    namespace {                                                            \
    EntityId _st_new_##ID(App* app) {                                      \
        return EntityNew<TYPE>(app).id;                                    \
    }                                                                      \
    void _st_click_##ID(void* self, Ctx* cx, int id) {                     \
        TYPE::Click((TYPE*)self, cx, id);                                  \
    }                                                                      \
    struct _StReg_##ID {                                                   \
        _StReg_##ID() { StoryRegister(ID, _st_new_##ID, _st_click_##ID); } \
    } _st_reg_##ID;                                                        \
    }
