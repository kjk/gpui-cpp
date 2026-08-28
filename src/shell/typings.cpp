#include "shell/typings.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

namespace gpui::shell {

constexpr int kTypesMaxEntries = kShellTypesMaxFiles + 1;
constexpr int kTypesMaxSourceBytes = 8 * 1024 * 1024;
constexpr int kTypesMaxDeclarationBytes = 2 * 1024 * 1024;

struct TypesDirectory {
    char path[kMaxPath] = {};
    int depth = 0;
};

static bool JoinPath(char* out, int cap, Str directory, Str name) {
    if (!out || cap <= 0 || !directory || !name) return false;
    bool separator = directory.s[directory.len - 1] != '/' &&
                     directory.s[directory.len - 1] != '\\';
    int len = directory.len + (separator ? 1 : 0) + name.len;
    if (len >= cap) return false;
    memcpy(out, directory.s, (size_t)directory.len);
    int at = directory.len;
    if (separator) out[at++] = GPUI_OS_WINDOWS ? '\\' : '/';
    memcpy(out + at, name.s, (size_t)name.len);
    out[len] = 0;
    return true;
}

static bool ReadBounded(Str path, int limit, Str* out) {
    *out = {};
    if (!path || path.len >= kMaxPath) return false;
    char name[kMaxPath];
    memcpy(name, path.s, (size_t)path.len);
    name[path.len] = 0;
    FILE* file = fopen(name, "rb");
    if (!file) return false;
    Vec<char> bytes;
    char block[16384];
    bool ok = true;
    for (;;) {
        size_t count = fread(block, 1, sizeof(block), file);
        if (count > 0) {
            char* destination = bytes.AppendBlanks((int)count);
            if (bytes.len > limit || !destination) {
                ok = false;
                break;
            }
            memcpy(destination, block, count);
        }
        if (count != sizeof(block)) {
            if (ferror(file)) ok = false;
            break;
        }
    }
    fclose(file);
    if (!ok) {
        bytes.Reset();
        return false;
    }
    int len = bytes.len;
    char* data = bytes.els;
    bytes.els = nullptr;
    bytes.len = bytes.cap = 0;
    *out = Str(data, len);
    return true;
}

static bool SourceImportsBuiltins(Str source) {
    static const char* specifiers[] = {"gpui", "gpui-base", "gpui-shell",
                                       "gpui-fps"};
    char quoted[32];
    for (int i = 0; i < 4; i++) {
        snprintf(quoted, sizeof(quoted), "\"%s\"", specifiers[i]);
        if (StrContains(source, Str(quoted))) return true;
        snprintf(quoted, sizeof(quoted), "'%s'", specifiers[i]);
        if (StrContains(source, Str(quoted))) return true;
    }
    return false;
}

static bool IsScript(const char* name) {
    if (!name) return false;
    int len = (int)strlen(name);
    return (len > 3 && strcmp(name + len - 3, ".js") == 0) ||
           (len > 4 && strcmp(name + len - 4, ".mjs") == 0);
}

static bool SkipDirectory(const char* name) {
    return !name || name[0] == '.' || strcmp(name, "node_modules") == 0 ||
           strcmp(name, "target") == 0;
}

static bool AppendDirectory(Vec<TypesDirectory>* directories, Str path,
                            int depth) {
    if (!path || path.len >= kMaxPath) return false;
    TypesDirectory directory;
    memcpy(directory.path, path.s, (size_t)path.len);
    directory.path[path.len] = 0;
    directory.depth = depth;
    return directories->Append(directory);
}

static void AppendQuoted(StrBuilder* out, Str value) {
    for (int i = 0; i < value.len; i++) {
        char ch = value.s[i];
        if (ch == '\\' || ch == '"') out->AppendChar('\\');
        out->AppendChar(ch);
    }
}

static void AppendReindented(StrBuilder* out, Str declarations) {
    int common = INT_MAX;
    int at = 0;
    while (at < declarations.len) {
        int end = at;
        while (end < declarations.len && declarations.s[end] != '\n') end++;
        int first = at;
        while (first < end &&
               (declarations.s[first] == ' ' || declarations.s[first] == '\t'))
            first++;
        if (first < end && first - at < common) common = first - at;
        at = end + 1;
    }
    if (common == INT_MAX) common = 0;
    at = 0;
    while (at < declarations.len) {
        int lineEnd = at;
        while (lineEnd < declarations.len && declarations.s[lineEnd] != '\n')
            lineEnd++;
        int end = lineEnd;
        while (end > at &&
               (declarations.s[end - 1] == ' ' ||
                declarations.s[end - 1] == '\t'))
            end--;
        if (end == at) {
            out->AppendChar('\n');
        } else {
            int first = at;
            int remove = common;
            while (first < end && remove > 0 &&
                   (declarations.s[first] == ' ' ||
                    declarations.s[first] == '\t')) {
                first++;
                remove--;
            }
            out->Append(StrL("  "));
            out->Append(Str(declarations.s + first, end - first));
            out->AppendChar('\n');
        }
        at = lineEnd + 1;
    }
}

void ShellTypeDeclarations(StrBuilder* out, const HostModules* modules) {
    if (!out) return;
    AppendBuiltinTypeDeclarations(out);
    for (int i = 0; i < HostModulesCount(modules); i++) {
        HostModule* module = HostModulesAt(modules, i);
        if (!module) continue;
        out->Append(StrL("\ndeclare module \""));
        AppendQuoted(out, module->Name());
        out->Append(StrL("\" {\n"));
        if (module->Declared()) {
            AppendReindented(out, module->Declared());
        } else {
            out->Append(StrL("  import { HostValue } from \"gpui\";\n\n"));
            for (int function = 0; function < module->FunctionCount();
                 function++) {
                Str name = module->FunctionName(function);
                out->Append(StrL("  export function "));
                out->Append(name);
                out->Append(StrL("(...args: HostValue[]): "));
                if (module->IsAsync(name))
                    out->Append(StrL("Promise<HostValue>"));
                else
                    out->Append(StrL("HostValue"));
                out->Append(StrL(";\n"));
            }
        }
        out->Append(StrL("}\n"));
    }
}

static bool HasSymlinkDeclaration(Str directory, DirEntry* entries,
                                  ShellError* error) {
    int count = PlatListDir(directory.s, entries, kTypesMaxEntries);
    if (count >= kTypesMaxEntries) {
        ShellErrorSet(error,
                      fmt("cannot safely inspect `%s` for an existing %s",
                          directory, Str(kShellTypesFile)));
        return true;
    }
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].name, kShellTypesFile) == 0 &&
            entries[i].isSymlink) {
            ShellErrorSet(error,
                          fmt("refusing to replace symlink `%s/%s`", directory,
                              Str(kShellTypesFile)));
            return true;
        }
    }
    return false;
}

static bool RefreshTypes(Str directory, Str declarations, DirEntry* entries,
                         bool* changed, ShellError* error) {
    if (changed) *changed = false;
    if (HasSymlinkDeclaration(directory, entries, error)) return false;
    char path[kMaxPath];
    if (!JoinPath(path, sizeof(path), directory, Str(kShellTypesFile))) {
        ShellErrorSet(error, StrL("type declaration path is too long"));
        return false;
    }
    Str current;
    if (ReadBounded(Str(path), kTypesMaxDeclarationBytes, &current) &&
        StrEq(current, declarations)) {
        StrFree(current);
        return true;
    }
    StrFree(current);
    FILE* file = fopen(path, "wb");
    if (!file) {
        ShellErrorSet(error, fmt("cannot write `%s`", Str(path)));
        return false;
    }
    size_t count = fwrite(declarations.s, 1, (size_t)declarations.len, file);
    bool ok = count == (size_t)declarations.len && fclose(file) == 0;
    if (!ok) {
        ShellErrorSet(error, fmt("cannot write `%s`", Str(path)));
        return false;
    }
    if (changed) *changed = true;
    return true;
}

bool ShellWriteTypeDeclarations(Str root, const HostModules* modules,
                                int* written, ShellError* error) {
    ShellErrorClear(error);
    if (written) *written = 0;
    if (!root || root.len >= kMaxPath) {
        ShellErrorSet(error, StrL("application directory is empty or too long"));
        return false;
    }
    TypesDirectory rootDirectory;
    memcpy(rootDirectory.path, root.s, (size_t)root.len);
    rootDirectory.path[root.len] = 0;
    if (!PlatDirExists(rootDirectory.path)) {
        ShellErrorSet(error, fmt("application directory `%s` does not exist",
                                 root));
        return false;
    }

    StrBuilder declarations;
    ShellTypeDeclarations(&declarations, modules);
    Str text = declarations.TakeStr();
    if (!text || text.len > kTypesMaxDeclarationBytes) {
        StrFree(text);
        ShellErrorSet(error, StrL("type declarations exceed the size limit"));
        return false;
    }

    Vec<TypesDirectory> pending;
    Vec<TypesDirectory> targets;
    bool ok = pending.Append(rootDirectory) && targets.Append(rootDirectory);
    DirEntry* entries = ok ? AllocArray<DirEntry>(kTypesMaxEntries) : nullptr;
    if (!entries) ok = false;
    int files = 0;
    while (ok && pending.len > 0 && files <= kShellTypesMaxFiles) {
        TypesDirectory directory = pending[pending.len - 1];
        pending.len--;
        int count = PlatListDir(directory.path, entries, kTypesMaxEntries);
        if (count >= kTypesMaxEntries) break;
        bool imports = false;
        for (int i = 0; i < count && files <= kShellTypesMaxFiles; i++) {
            const DirEntry& item = entries[i];
            if (item.isSymlink || item.name[0] == '.') continue;
            if (item.isDir) {
                if (directory.depth >= kShellTypesMaxDepth ||
                    SkipDirectory(item.name))
                    continue;
                char child[kMaxPath];
                if (!JoinPath(child, sizeof(child), Str(directory.path),
                              Str(item.name)) ||
                    !AppendDirectory(&pending, Str(child),
                                     directory.depth + 1)) {
                    ok = false;
                    break;
                }
            } else if (item.isFile) {
                files++;
                if (imports || !IsScript(item.name)) continue;
                char sourcePath[kMaxPath];
                Str source;
                if (JoinPath(sourcePath, sizeof(sourcePath),
                             Str(directory.path), Str(item.name)) &&
                    ReadBounded(Str(sourcePath), kTypesMaxSourceBytes,
                                &source)) {
                    imports = SourceImportsBuiltins(source);
                }
                StrFree(source);
            }
        }
        if (imports && !StrEq(Str(directory.path), root))
            ok = AppendDirectory(&targets, Str(directory.path),
                                 directory.depth);
    }

    for (int i = 0; ok && i < targets.len; i++) {
        bool changed = false;
        ok = RefreshTypes(Str(targets[i].path), text, entries, &changed, error);
        if (ok && changed && written) (*written)++;
    }
    if (!ok && error && !error->IsSet())
        ShellErrorSet(error,
                      StrL("out of memory while writing type declarations"));
    Free(nullptr, entries);
    targets.Reset();
    pending.Reset();
    StrFree(text);
    return ok;
}

} // namespace gpui::shell
