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

// Runs one explicitly granted executable with an empty child environment,
// captures each output stream up to 8 MiB, and kills the process tree after
// 30 seconds. This is blocking and belongs on ExecSpawn's worker half.
bool ProcessRunBounded(Str command, const Str* args, int count,
                       ProcessCancellation* cancellation, ProcessOutput* out,
                       Str* error);

} // namespace gpui::shell
#endif // GPUI_SHELL_PROCESS_H_
