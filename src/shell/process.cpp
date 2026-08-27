#include "shell/process.h"

namespace gpui::shell {

void ProcessCancellation::Cancel() {
    mutex.Lock();
    cancelled = true;
    mutex.Unlock();
}

bool ProcessCancellation::IsCancelled() {
    mutex.Lock();
    bool result = cancelled;
    mutex.Unlock();
    return result;
}

void ProcessOutput::Free() {
    StrFree(out);
    StrFree(err);
    *this = {};
}

} // namespace gpui::shell
