#ifndef GPUI_SHELL_FILESYSTEM_H_
#define GPUI_SHELL_FILESYSTEM_H_

#include "base.h"

namespace gpui::shell {

constexpr int kFsMaxReadBytes = 64 * 1024 * 1024;
constexpr int kFsMaxWriteBytes = 8 * 1024 * 1024;
constexpr int kFsMaxDirectoryEntries = 10000;
constexpr int kFsMaxDirectoryNameBytes = 1024 * 1024;

enum class FsOperation : uint8_t {
    Read,
    Write,
    ReadDirectory,
    Exists,
    RemoveFile,
    RemoveDirectory,
    MakeDirectory,
};

struct FsEntry {
    Str name;
    bool isDirectory = false;
};

struct FsResult {
    Str bytes;
    Vec<FsEntry> entries;
    bool exists = false;

    void Free();
};

// `root` is the granted directory and `relative` is the normalized name
// inside it. The platform implementation resolves every component through a
// directory handle and refuses symlinks/reparse points, so this check and the
// operation are one authority-preserving traversal rather than a string test
// followed by an ambient path syscall.
bool FsRun(FsOperation operation, Str root, Str relative, Str input,
           bool recursive, FsResult* result, Str* error);

} // namespace gpui::shell
#endif // GPUI_SHELL_FILESYSTEM_H_
