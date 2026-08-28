#ifndef GPUI_SHELL_WATCH_H_
#define GPUI_SHELL_WATCH_H_

#include "shell/view.h"

namespace gpui::shell {

constexpr int kSourceWatchMaxDepth = 8;
constexpr int kSourceWatchMaxFiles = 4096;
constexpr int kSourceWatchDebounceMs = 200;
constexpr int kSourceWatchPollMs = 250;

struct SourceTreeStamp {
    uint64_t newest = 0;
    uint64_t bytes = 0;
    uint32_t files = 0;

    bool operator==(const SourceTreeStamp& other) const {
        return newest == other.newest && bytes == other.bytes &&
               files == other.files;
    }
    bool operator!=(const SourceTreeStamp& other) const {
        return !(*this == other);
    }
};

bool ScanSourceTree(Str directory, SourceTreeStamp* stamp,
                    ShellError* error = nullptr,
                    int maxFiles = kSourceWatchMaxFiles);

class SourceWatcher {
  public:
    SourceWatcher() = default;
    SourceWatcher(const SourceWatcher&) = delete;
    SourceWatcher& operator=(const SourceWatcher&) = delete;
    ~SourceWatcher();

    bool Init(Str directory, ShellError* error = nullptr,
              int debounceMs = kSourceWatchDebounceMs);
    bool Poll(bool* changed, ShellError* error = nullptr);
    bool PollAt(double now, bool* changed, ShellError* error = nullptr);
    bool Observe(const SourceTreeStamp& next, double now, bool* changed);

  private:
    Str directory;
    SourceTreeStamp stamp;
    double changedAt = 0;
    int debounceMs = kSourceWatchDebounceMs;
    bool pending = false;
};

} // namespace gpui::shell

namespace gpui {

// A polling source watch tied to one ScriptView. The entity owns its timer;
// dropping it stops the watch.
struct ShellWatcher {
    ShellRuntime* runtime = nullptr;
    EntityId view = {};
    Window* window = nullptr;
    Str directory;
    Str entry;
    shell::SourceWatcher source;
    ShellError error = {};
    int timer = 0;
    int scanTask = 0;
    void* scanJob = nullptr;

    ~ShellWatcher();

    static Entity<ShellWatcher> Start(ShellRuntime* runtime,
                                      Entity<ScriptView> view, Str directory,
                                      Str entry, Window* window, App* app,
                                      ShellError* error = nullptr);
    static void OnPoll(ShellWatcher* self, Ctx* cx, const TickEvent*);
};

} // namespace gpui

#endif // GPUI_SHELL_WATCH_H_
