#include "shell/storage.h"

#include <errno.h>
#include <stdio.h>

namespace gpui::shell {

constexpr int kMaxStorageBytes = 8 * 1024 * 1024;
constexpr int kMaxStorageKeys = 4096;
constexpr int kMaxStorageValueBytes = 1024 * 1024;

static void StorageError(Str* error, Str message) {
    if (!error) return;
    StrFree(*error);
    *error = StrDup(message);
}

Storage::Storage(bool writes) : persisted(writes) {}

Storage::~Storage() {
    ResetEntries();
    StrFree(path);
}

void Storage::ResetEntries() {
    for (int i = 0; i < entries.len; i++) {
        StrFree(entries[i]->key);
        StrFree(entries[i]->value);
        delete entries[i];
    }
    entries.Reset();
}

bool Storage::SetPath(Str value, Str* error) {
    if (error) {
        StrFree(*error);
        *error = {};
    }
    ResetEntries();
    StrFree(path);
    path = StrDup(value);
    dirty = false;
    return Load(error);
}

bool Storage::Load(Str* error) {
    if (!persisted || !path) return true;
    FILE* file = fopen(path.s, "rb");
    if (!file) {
        if (errno == ENOENT) return true;
        StorageError(error, fmt("cannot read storage file `%s`", path));
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        StorageError(error, fmt("cannot read storage file `%s`", path));
        return false;
    }
    long size = ftell(file);
    if (size < 0 || size > kMaxStorageBytes || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        StorageError(error, fmt("storage file `%s` exceeds the 8 MiB limit", path));
        return false;
    }
    char* bytes = (char*)Alloc(nullptr, (int)size + 1);
    if (!bytes) {
        fclose(file);
        StorageError(error, StrL("allocating the storage file failed"));
        return false;
    }
    size_t got = fread(bytes, 1, (size_t)size, file);
    fclose(file);
    bytes[size] = 0;
    if (got != (size_t)size) {
        Free(nullptr, bytes);
        StorageError(error, fmt("cannot read storage file `%s`", path));
        return false;
    }
    Arena* arena = ArenaNew();
    JsonValue* root = JsonParse(arena, Str(bytes, (int)size));
    bool ok = root && root->kind == JsonKind::Object;
    for (JsonValue* item = ok ? root->first : nullptr; item; item = item->next) {
        if (item->kind != JsonKind::String || entries.len >= kMaxStorageKeys ||
            item->str.len > kMaxStorageValueBytes) {
            ok = false;
            break;
        }
        StorageEntry* entry = new StorageEntry();
        entry->key = StrDup(item->key);
        entry->value = StrDup(item->str);
        entries.Append(entry);
    }
    ArenaDelete(arena);
    Free(nullptr, bytes);
    if (!ok) {
        ResetEntries();
        StorageError(error, fmt("`%s` is not a valid shell storage file", path));
    }
    return ok;
}

Str Storage::Get(Str key) const {
    for (int i = 0; i < entries.len; i++) {
        if (StrEq(entries[i]->key, key)) return entries[i]->value;
    }
    return {};
}

bool Storage::Set(Str key, Str value, Str* error) {
    if (key.len > kMaxStorageValueBytes || value.len > kMaxStorageValueBytes) {
        StorageError(error, StrL("a storage key or value exceeds the 1 MiB limit"));
        return false;
    }
    for (int i = 0; i < entries.len; i++) {
        if (!StrEq(entries[i]->key, key)) continue;
        StrFree(entries[i]->value);
        entries[i]->value = StrDup(value);
        dirty = true;
        return true;
    }
    if (entries.len >= kMaxStorageKeys) {
        StorageError(error, StrL("storage reached its 4096-key limit"));
        return false;
    }
    StorageEntry* entry = new StorageEntry();
    entry->key = StrDup(key);
    entry->value = StrDup(value);
    entries.Append(entry);
    dirty = true;
    return true;
}

bool Storage::Remove(Str key, Str*) {
    for (int i = 0; i < entries.len; i++) {
        if (!StrEq(entries[i]->key, key)) continue;
        StorageEntry* entry = entries[i];
        for (int j = i + 1; j < entries.len; j++) entries[j - 1] = entries[j];
        entries.len--;
        StrFree(entry->key);
        StrFree(entry->value);
        delete entry;
        dirty = true;
        break;
    }
    return true;
}

bool Storage::Clear(Str*) {
    if (entries.len) {
        ResetEntries();
        dirty = true;
    }
    return true;
}

Str Storage::Key(int index) const {
    return index >= 0 && index < entries.len ? entries[index]->key : Str{};
}

bool Storage::Flush(Str* error) {
    if (!persisted || !dirty) return true;
    if (!path) {
        StorageError(error, StrL("localStorage has no backing file; call ShellSetStoragePath first"));
        return false;
    }
    StrBuilder body;
    JsonWriter writer;
    writer.out = &body;
    writer.BeginObject();
    for (int i = 0; i < entries.len; i++) {
        writer.String(entries[i]->key.s, entries[i]->value);
    }
    writer.EndObject();
    Str encoded = body.TakeStr();
    if (encoded.len > kMaxStorageBytes) {
        StorageError(error, StrL("the encoded storage file exceeds the 8 MiB limit"));
        StrFree(encoded);
        return false;
    }
    Str temporary = StrDup(fmt("%s.tmp", path));
    FILE* file = fopen(temporary.s, "wb");
    bool ok = file && fwrite(encoded.s, 1, (size_t)encoded.len, file) ==
                               (size_t)encoded.len;
    if (file && fclose(file) != 0) ok = false;
    if (ok) ok = StorageReplaceFile(temporary, path, error);
    if (!ok) {
        remove(temporary.s);
        if (!error || !*error)
            StorageError(error, fmt("cannot write storage file `%s`", path));
    } else {
        dirty = false;
    }
    StrFree(temporary);
    StrFree(encoded);
    return ok;
}

} // namespace gpui::shell
