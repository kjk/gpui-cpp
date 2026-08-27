#ifndef GPUI_SHELL_STORAGE_H_
#define GPUI_SHELL_STORAGE_H_

#include "base/json.h"

namespace gpui::shell {

bool StorageReplaceFile(Str temporary, Str path, Str* error);

struct StorageEntry {
    Str key;
    Str value;
};

class Storage {
  public:
    explicit Storage(bool persisted = false);
    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;
    ~Storage();

    bool SetPath(Str path, Str* error = nullptr);
    Str Get(Str key) const;
    bool Set(Str key, Str value, Str* error = nullptr);
    bool Remove(Str key, Str* error = nullptr);
    bool Clear(Str* error = nullptr);
    int Len() const { return entries.len; }
    Str Key(int index) const;
    bool Flush(Str* error = nullptr);
    bool HasPath() const { return path.s != nullptr; }

  private:
    Vec<StorageEntry*> entries;
    Str path;
    bool persisted = false;
    bool dirty = false;

    bool Load(Str* error);
    void ResetEntries();
};

} // namespace gpui::shell
#endif // GPUI_SHELL_STORAGE_H_
