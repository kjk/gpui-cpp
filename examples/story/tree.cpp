#include "Story.h"

// One row of the file tree; the Rust story reads ./ the same way.
struct TreeEntry {
    char name[128] = {};
    bool folder = false;
};

struct TreeStory {
    TreeEntry entries[256] = {};
    int n = 0;
    int selected = -1;
    bool loaded = false;

    static El* Render(TreeStory* self, Ctx* cx);
};

static void SelectTreeItem(TreeStory* self, Ctx* cx, const ClickEvent*,
                           intptr_t ix) {
    self->selected = (int)ix;
    Notify(cx);
}
static void SelectRandom(TreeStory* self, Ctx* cx, const ClickEvent*) {
    if (self->n > 0) {
        self->selected = (int)(GetTickCount() % (DWORD)self->n);
    }
    Notify(cx);
}

// Rust filters with the repo gitignore; ours skips the same few by name.
static bool TreeSkip(const char* name) {
    static const char* kSkip[] = {".git", "out", "node_modules", ".work"};
    for (size_t i = 0; i < sizeof(kSkip) / sizeof(kSkip[0]); i++) {
        if (strcmp(name, kSkip[i]) == 0) {
            return true;
        }
    }
    return false;
}

static void LoadTree(TreeStory* self) {
    WIN32_FIND_DATAW fd = {};
    HANDLE h = FindFirstFileW(L"./*", &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if (fd.cFileName[0] == L'.' && fd.cFileName[1] == 0) {
            continue;
        }
        if (fd.cFileName[0] == L'.' && fd.cFileName[1] == L'.' &&
            fd.cFileName[2] == 0) {
            continue;
        }
        if (self->n >= 256) {
            break;
        }
        TreeEntry& e = self->entries[self->n];
        WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, e.name,
                            (int)sizeof(e.name), nullptr, nullptr);
        if (TreeSkip(e.name)) {
            continue;
        }
        e.folder = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        self->n++;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    // Folders first, then by name.
    for (int i = 1; i < self->n; i++) {
        TreeEntry key = self->entries[i];
        int j = i - 1;
        while (j >= 0) {
            const TreeEntry& cur = self->entries[j];
            bool after = cur.folder != key.folder
                             ? (!cur.folder && key.folder)
                             : strcmp(cur.name, key.name) > 0;
            if (!after) {
                break;
            }
            self->entries[j + 1] = self->entries[j];
            j--;
        }
        self->entries[j + 1] = key;
    }
}

El* TreeStory::Render(TreeStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    if (!self->loaded) {
        self->loaded = true;
        LoadTree(self);
    }
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);

    El* btnRow = Div(a)->FlexRow()->Gap(12);
    btnRow->Child(component::Button::New(cx, StrL("select-item"))
                      ->Label(StrL("Select Item"))
                      ->Outline()
                      ->OnClick(Listen(cx, &SelectRandom))
                      ->IntoEl());
    page->Child(btnRow);

    El* sec = StorySection(cx, "File tree", nullptr);
    StorySectionSubTitle(
        sec, StoryTxt(cx,
                      StrL("Press `enter` to rename. Right-click for context "
                           "menu."),
                      16, th.mutedFg));
    El* col = Div(a)->FlexCol()->W(480)->Gap(16);
    El* box = Div(a)
                  ->FlexCol()
                  ->W(kFill)
                  ->H(540)
                  ->Pad(4)
                  ->ClipY()
                  ->Radius(th.radius)
                  ->Border(1, th.border);
    Listener pick = Listen(cx, &SelectTreeItem);
    for (int i = 0; i < self->n; i++) {
        const TreeEntry& e = self->entries[i];
        El* row = Div(a)
                      ->FlexRow()
                      ->W(kFill)
                      ->H(34)
                      ->PadR(12)
                      // px_3 plus 16 per depth; every entry here is depth 0.
                      ->PadL(12)
                      ->Gap(8)
                      ->ItemsCenter()
                      ->Radius(th.radius)
                      ->HoverBg(th.muted);
        if (i == self->selected) {
            row->Bg(th.accent);
        }
        row->Child(IconEl(a, e.folder ? IconName::Folder : IconName::File, 16)
                       ->Fg(th.foreground));
        row->Child(StoryTxt(cx, StoryDup(cx, e.name), 16, th.foreground));
        row->Click(HashClickId(StoryFmt(cx, "tree-%d", i)))
            ->OnClick(ListenerArg(pick, i));
        box->Child(row);
    }
    col->Child(box);
    El* status = Div(a)->FlexRow()->W(kFill)->Gap(12)->JustifyBetween();
    if (self->selected >= 0) {
        status->Child(
            StoryTxt(cx, StoryFmt(cx, "Selected Index: %d", self->selected), 16,
                     th.foreground));
        status->Child(
            component::Label::New(cx, StrL("Selected:"))
                ->Secondary(StoryDup(cx, self->entries[self->selected].name))
                ->IntoEl());
    }
    col->Child(status);
    StorySectionAdd(sec, col);
    page->Child(sec);
    return page;
}

STORY_PAGE(StoryTree, TreeStory);
