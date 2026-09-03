#ifndef GPUI_SHELL_PROCESS_H_
#define GPUI_SHELL_PROCESS_H_

#include "base.h"

namespace gpui::shell {

struct ProcessCancellation {
    Mutex mutex;
    bool cancelled = false;

    void Cancel();
    bool IsCancelled();
};

struct ProcessOutput {
    int code = -1;
    Str out;
    Str err;

    void Free();
};

// What a host-owned run needs beyond `process.run`'s. Scripts never reach
// this: the dependency fetcher runs `git` in a cache directory, and git wants
// the host environment the way Rust's `Command` gives it, plus the two
// variables that keep a credential prompt from blocking a background fetch.
struct ProcessOptions {
    // Empty: the child inherits the host's working directory.
    Str workingDirectory;
    // "NAME=VALUE" pairs added to whichever environment is selected below.
    const Str* environment = nullptr;
    int environmentCount = 0;
    // False — `process.run`'s contract — clears the child environment.
    bool inheritEnvironment = false;
};

// Runs one explicitly granted executable with an empty child environment,
// captures each output stream up to 8 MiB, and kills the process tree after
// 30 seconds. This is blocking and belongs on ExecSpawn's worker half.
bool ProcessRunBounded(Str command, const Str* args, int count,
                       ProcessCancellation* cancellation, ProcessOutput* out,
                       Str* error, const ProcessOptions* options = nullptr);

} // namespace gpui::shell
#endif // GPUI_SHELL_PROCESS_H_
