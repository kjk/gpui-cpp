#include "shell/plugin.h"

#include "base/json.h"
#include "shell/filesystem.h"
#include "shell/scope.h"

#include <stdio.h>
#include <stdlib.h>

namespace gpui::shell {

static void SetError(ShellError* error, Str message) {
    ShellErrorSet(error, message);
}

static Str Join(Arena* arena, Str left, Str right) {
    StrBuilder path;
    path.a = arena;
    path.Append(left);
    if (left && left.s[left.len - 1] != '/' && left.s[left.len - 1] != '\\')
        path.AppendChar(GPUI_OS_WINDOWS ? '\\' : '/');
    path.Append(right);
    return path.TakeStr();
}

static bool ReadBoundedFile(Str path, int limit, Str* out) {
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
        size_t read = fread(block, 1, sizeof(block), file);
        if (read > 0) {
            if (bytes.len > limit - (int)read) {
                ok = false;
                break;
            }
            memcpy(bytes.AppendBlanks((int)read), block, read);
        }
        if (read != sizeof(block)) {
            if (ferror(file)) ok = false;
            break;
        }
    }
    fclose(file);
    if (ok) {
        int size = bytes.len;
        char* data = bytes.els;
        bytes.els = nullptr;
        bytes.len = 0;
        bytes.cap = 0;
        *out = Str(data, size);
    }
    bytes.Reset();
    return ok;
}

static const char* JsonTypeName(const JsonValue* value) {
    if (!value) return "missing";
    switch (value->kind) {
        case JsonKind::Null: return "null";
        case JsonKind::Bool: return "a boolean";
        case JsonKind::Number: return "a number";
        case JsonKind::String: return "a string";
        case JsonKind::Array: return "an array";
        case JsonKind::Object: return "an object";
    }
    return "a value";
}

static bool HasOnly(const JsonValue* object, const char* const* names,
                    int count, Str where, ShellError* error) {
    if (!object || object->kind != JsonKind::Object) {
        SetError(error, fmt("%s must be an object, found %s", where,
                            Str(JsonTypeName(object))));
        return false;
    }
    for (const JsonValue* field = object->first; field; field = field->next) {
        bool known = false;
        for (int i = 0; i < count; i++)
            if (StrEq(field->key, names[i])) known = true;
        if (!known) {
            SetError(error,
                     fmt("unknown field `%s` in %s", field->key, where));
            return false;
        }
    }
    return true;
}

static bool RequiredString(const JsonValue* object, const char* field,
                           Str* out, ShellError* error) {
    const JsonValue* value = JsonGet(object, field);
    if (!value || value->kind == JsonKind::Null) {
        SetError(error, fmt("missing field `%s`", Str(field)));
        return false;
    }
    if (value->kind != JsonKind::String) {
        SetError(error,
                 fmt("field `%s` must be a string, found %s", Str(field),
                     Str(JsonTypeName(value))));
        return false;
    }
    if (value->str.len == 0) {
        SetError(error, fmt("field `%s` is empty", Str(field)));
        return false;
    }
    *out = value->str;
    return true;
}

static bool ParseSemver(Str value, int* major, int* minor, int* patch) {
    int parts[3] = {};
    int at = 0;
    for (int part = 0; part < 3; part++) {
        if (at >= value.len || value.s[at] < '0' || value.s[at] > '9')
            return false;
        if (value.s[at] == '0' && at + 1 < value.len &&
            value.s[at + 1] >= '0' && value.s[at + 1] <= '9')
            return false;
        int number = 0;
        while (at < value.len && value.s[at] >= '0' && value.s[at] <= '9') {
            if (number > 100000000) return false;
            number = number * 10 + value.s[at++] - '0';
        }
        parts[part] = number;
        if (part < 2 && (at >= value.len || value.s[at++] != '.'))
            return false;
    }
    if (at < value.len) {
        if (value.s[at] != '-' && value.s[at] != '+') return false;
        for (; at < value.len; at++) {
            char ch = value.s[at];
            if (!((ch >= 'a' && ch <= 'z') ||
                  (ch >= 'A' && ch <= 'Z') ||
                  (ch >= '0' && ch <= '9') || ch == '-' || ch == '.' ||
                  ch == '+'))
                return false;
        }
    }
    if (major) *major = parts[0];
    if (minor) *minor = parts[1];
    if (patch) *patch = parts[2];
    return true;
}

static bool ValidId(Str id) {
    if (!id) return false;
    for (int i = 0; i < id.len; i++) {
        char ch = id.s[i];
        if (!((ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '.' || ch == '-' ||
              ch == '_'))
            return false;
    }
    char first = id.s[0];
    char last = id.s[id.len - 1];
    if (first == '.' || first == '-' || first == '_' || last == '.' ||
        last == '-' || last == '_')
        return false;
    return StrFind(id, StrL("..")) < 0;
}

static bool ValidEntry(Str entry) {
    if (!entry || entry.s[0] == '/' || entry.s[0] == '\\' ||
        StrFind(entry, StrL(":")) >= 0)
        return false;
    int start = 0;
    for (int i = 0; i <= entry.len; i++) {
        if (i < entry.len && entry.s[i] != '/' && entry.s[i] != '\\')
            continue;
        if (i - start == 2 && entry.s[start] == '.' &&
            entry.s[start + 1] == '.')
            return false;
        start = i + 1;
    }
    return true;
}

static bool ParseStringArray(Arena* arena, const JsonValue* value,
                             Vec<Str>* out, Str field, ShellError* error,
                             bool required = false) {
    if (!value) {
        if (required) SetError(error, fmt("%s must be an array", field));
        return !required;
    }
    if (value->kind != JsonKind::Array) {
        SetError(error, fmt("%s must be an array", field));
        return false;
    }
    for (const JsonValue* item = value->first; item; item = item->next) {
        if (item->kind != JsonKind::String || !item->str) {
            SetError(error, fmt("%s entries must be non-empty strings", field));
            return false;
        }
        out->Append(StrDup(arena, item->str));
    }
    return true;
}

static bool ValidatePlaceholders(const Vec<Str>& paths, Str field,
                                 ShellError* error) {
    for (int p = 0; p < paths.len; p++) {
        Str value = paths[p];
        for (int i = 0; i + 2 < value.len; i++) {
            if (value.s[i] != '$' || value.s[i + 1] != '{') continue;
            int end = i + 2;
            while (end < value.len && value.s[end] != '}') end++;
            if (end >= value.len) {
                SetError(error, fmt("unterminated placeholder in %s", field));
                return false;
            }
            Str placeholder(value.s + i, end - i + 1);
            if (!StrEq(placeholder, "${pluginDir}") &&
                !StrEq(placeholder, "${dataDir}")) {
                SetError(error, fmt("unknown placeholder `%s` in %s", placeholder,
                                    field));
                return false;
            }
            i = end;
        }
    }
    return true;
}

static bool ParseCapabilities(const JsonValue* value, PluginManifest* out,
                              ShellError* error) {
    if (!value || value->kind == JsonKind::Null) return true;
    static const char* fields[] = {"fs", "network", "storage", "clipboard",
                                   "process"};
    if (!HasOnly(value, fields, 5, StrL("capabilities"), error)) return false;
    const JsonValue* storage = JsonGet(value, "storage");
    if (storage) {
        if (storage->kind != JsonKind::Bool) {
            SetError(error, StrL("capabilities.storage must be a boolean"));
            return false;
        }
        out->storage = storage->b;
    }
    const JsonValue* fs = JsonGet(value, "fs");
    if (fs && fs->kind != JsonKind::Null) {
        static const char* fsFields[] = {"read", "write", "execute"};
        if (!HasOnly(fs, fsFields, 3, StrL("capabilities.fs"), error) ||
            !ParseStringArray(out->arena, JsonGet(fs, "read"),
                              &out->readRoots,
                              StrL("capabilities.fs.read"), error) ||
            !ParseStringArray(out->arena, JsonGet(fs, "write"),
                              &out->writeRoots,
                              StrL("capabilities.fs.write"), error))
            return false;
        if (!ValidatePlaceholders(out->readRoots,
                                  StrL("capabilities.fs.read"), error) ||
            !ValidatePlaceholders(out->writeRoots,
                                  StrL("capabilities.fs.write"), error))
            return false;
        const JsonValue* execute = JsonGet(fs, "execute");
        if (execute && execute->kind == JsonKind::String &&
            StrEq(execute->str, "*")) {
            out->executeUnrestricted = true;
        } else if (execute &&
                   !ParseStringArray(out->arena, execute, &out->execute,
                                     StrL("capabilities.fs.execute"), error)) {
            return false;
        }
    }
    const JsonValue* network = JsonGet(value, "network");
    if (network && network->kind != JsonKind::Null) {
        static const char* networkFields[] = {"hosts", "http"};
        if (!HasOnly(network, networkFields, 2,
                     StrL("capabilities.network"), error) ||
            !ParseStringArray(out->arena, JsonGet(network, "hosts"),
                              &out->networkHosts,
                              StrL("capabilities.network.hosts"), error))
            return false;
        for (int i = 0; i < out->networkHosts.len; i++) {
            Str host = out->networkHosts[i];
            if (!host || StrFind(host, StrL("://")) >= 0 ||
                StrFind(host, StrL("/")) >= 0) {
                SetError(error, fmt("network host `%s` must be a hostname without a scheme or path", host));
                return false;
            }
        }
        const JsonValue* http = JsonGet(network, "http");
        if (http && http->kind != JsonKind::Array) {
            SetError(error, StrL("capabilities.network.http must be an array"));
            return false;
        }
        for (const JsonValue* rule = http ? http->first : nullptr; rule;
             rule = rule->next) {
            static const char* httpFields[] = {
                "scheme", "host", "port", "methods", "paths",
                "path_prefixes"};
            if (!HasOnly(rule, httpFields, 6,
                         StrL("capabilities.network.http entry"), error))
                return false;
            auto* parsed = ArenaNew<PluginHttpGrant>(out->arena);
            const JsonValue* scheme = JsonGet(rule, "scheme");
            parsed->scheme = StrDup(out->arena,
                                    scheme && scheme->kind == JsonKind::String
                                        ? scheme->str
                                        : StrL("https"));
            Str host;
            if (!RequiredString(rule, "host", &host, error)) return false;
            parsed->host = StrDup(out->arena, host);
            if ((!StrEq(parsed->scheme, "http") &&
                 !StrEq(parsed->scheme, "https")) ||
                StrFind(host, StrL("://")) >= 0 ||
                StrFind(host, StrL("/")) >= 0) {
                SetError(error, StrL("invalid capabilities.network.http scheme or host"));
                return false;
            }
            const JsonValue* port = JsonGet(rule, "port");
            if (port) {
                if (port->kind != JsonKind::Number || port->num < 1 ||
                    port->num > 65535 || port->num != (int)port->num) {
                    SetError(error, StrL("capabilities.network.http port must be 1..65535"));
                    return false;
                }
                parsed->hasPort = true;
                parsed->port = (uint16_t)port->num;
            }
            if (!ParseStringArray(out->arena, JsonGet(rule, "methods"),
                                  &parsed->methods,
                                  StrL("capabilities.network.http.methods"),
                                  error, true) ||
                parsed->methods.len == 0 ||
                !ParseStringArray(out->arena, JsonGet(rule, "paths"),
                                  &parsed->paths,
                                  StrL("capabilities.network.http.paths"), error) ||
                !ParseStringArray(out->arena,
                                  JsonGet(rule, "path_prefixes"),
                                  &parsed->pathPrefixes,
                                  StrL("capabilities.network.http.path_prefixes"), error))
                return false;
            for (int i = 0; i < parsed->methods.len; i++) {
                if (!StrEq(parsed->methods[i], "GET") &&
                    !StrEq(parsed->methods[i], "POST")) {
                    SetError(error, fmt("invalid HTTP method `%s`", parsed->methods[i]));
                    return false;
                }
            }
            for (int pass = 0; pass < 2; pass++) {
                Vec<Str>& paths = pass ? parsed->pathPrefixes : parsed->paths;
                for (int i = 0; i < paths.len; i++) {
                    if (!paths[i] || paths[i].s[0] != '/') {
                        SetError(error, StrL("HTTP grant paths must start with `/`"));
                        return false;
                    }
                }
            }
            out->http.Append(parsed);
        }
    }
    const JsonValue* clipboard = JsonGet(value, "clipboard");
    if (clipboard && clipboard->kind != JsonKind::Null) {
        static const char* clipboardFields[] = {"read", "write"};
        if (!HasOnly(clipboard, clipboardFields, 2,
                     StrL("capabilities.clipboard"), error))
            return false;
        const JsonValue* read = JsonGet(clipboard, "read");
        const JsonValue* write = JsonGet(clipboard, "write");
        if ((read && read->kind != JsonKind::Bool) ||
            (write && write->kind != JsonKind::Bool)) {
            SetError(error, StrL("clipboard grants must be booleans"));
            return false;
        }
        out->clipboardRead = read && read->b;
        out->clipboardWrite = write && write->b;
    }
    const JsonValue* process = JsonGet(value, "process");
    if (process && process->kind != JsonKind::Null) {
        static const char* processFields[] = {"exit"};
        if (!HasOnly(process, processFields, 1,
                     StrL("capabilities.process"), error))
            return false;
        const JsonValue* exit = JsonGet(process, "exit");
        if (exit && exit->kind != JsonKind::Bool) {
            SetError(error, StrL("capabilities.process.exit must be a boolean"));
            return false;
        }
        out->exit = exit && exit->b;
    }
    return true;
}

PluginManifest::PluginManifest() : arena(ArenaNew()) {}

PluginManifest::~PluginManifest() {
    readRoots.Reset();
    writeRoots.Reset();
    execute.Reset();
    networkHosts.Reset();
    for (int i = 0; i < http.len; i++) {
        http[i]->methods.Reset();
        http[i]->paths.Reset();
        http[i]->pathPrefixes.Reset();
    }
    http.Reset();
    ArenaDelete(arena);
}

bool PluginManifestParse(Str source, PluginManifest* out, ShellError* error) {
    ShellErrorClear(error);
    if (!out || !out->arena) {
        SetError(error, StrL("manifest output is not initialized"));
        return false;
    }
    JsonValue* root = JsonParse(out->arena, source);
    if (!root) {
        SetError(error, StrL("the manifest is not valid JSON"));
        return false;
    }
    static const char* fields[] = {"id", "name", "version",
                                   "shell-version", "entry", "capabilities"};
    if (!HasOnly(root, fields, 6, StrL("the manifest"), error)) return false;
    Str id, name, entry;
    if (!RequiredString(root, "id", &id, error) ||
        !RequiredString(root, "name", &name, error) ||
        !RequiredString(root, "entry", &entry, error))
        return false;
    if (!ValidId(id)) {
        SetError(error, fmt("invalid `id` `%s`: use lowercase letters, digits, `.`, `-` and `_`, beginning and ending with a letter or digit", id));
        return false;
    }
    if (!ValidEntry(entry)) {
        SetError(error, fmt("invalid `entry` `%s`: expected a path inside the plugin directory", entry));
        return false;
    }
    const JsonValue* version = JsonGet(root, "version");
    Str versionText = version && version->kind != JsonKind::Null
                          ? JsonString(version)
                          : StrL("unknown");
    if ((version && version->kind != JsonKind::Null && !versionText) ||
        (versionText && !StrEq(versionText, "unknown") &&
         !ParseSemver(versionText, nullptr, nullptr, nullptr))) {
        SetError(error, fmt("invalid `version` `%s`: expected a semantic version", versionText));
        return false;
    }
    const JsonValue* shellVersion = JsonGet(root, "shell-version");
    Str required = shellVersion && shellVersion->kind != JsonKind::Null
                       ? JsonString(shellVersion)
                       : Str(kShellVersion);
    int requiredMajor = 0, requiredMinor = 0, requiredPatch = 0;
    int runtimeMajor = 0, runtimeMinor = 0, runtimePatch = 0;
    if (!required ||
        !ParseSemver(required, &requiredMajor, &requiredMinor,
                     &requiredPatch)) {
        SetError(error, fmt("invalid `shell-version` `%s`: expected a semantic version", required));
        return false;
    }
    ParseSemver(Str(kShellVersion), &runtimeMajor, &runtimeMinor,
                &runtimePatch);
    bool line = requiredMajor == 0
                    ? runtimeMajor == 0 && runtimeMinor == requiredMinor
                    : runtimeMajor == requiredMajor;
    bool oldEnough = runtimeMajor > requiredMajor ||
                     (runtimeMajor == requiredMajor &&
                      (runtimeMinor > requiredMinor ||
                       (runtimeMinor == requiredMinor &&
                        runtimePatch >= requiredPatch)));
    if (!line || !oldEnough) {
        SetError(error, fmt("this application requires gpui-shell %s, but this runtime is %s and is not compatible", required, Str(kShellVersion)));
        return false;
    }
    out->id = StrDup(out->arena, id);
    out->name = StrDup(out->arena, name);
    out->version = StrDup(out->arena, versionText);
    out->shellVersion = StrDup(out->arena, required);
    out->entry = StrDup(out->arena, entry);
    return ParseCapabilities(JsonGet(root, "capabilities"), out, error);
}

bool PluginManifestRead(Str directory, PluginManifest* out,
                        ShellError* error) {
    Arena* scratch = ArenaNew();
    Str path = Join(scratch, directory, Str(kShellManifestFile));
    Str source;
    if (!ReadBoundedFile(path, kShellMaxManifestBytes, &source)) {
        SetError(error, fmt("%s: cannot read the manifest", path));
        ArenaDelete(scratch);
        return false;
    }
    bool ok = PluginManifestParse(source, out, error);
    if (!ok && error && error->message) {
        Str old = error->message;
        error->message = StrDup(fmt("%s: %s", path, old));
        StrFree(old);
    }
    StrFree(source);
    ArenaDelete(scratch);
    return ok;
}

void PluginManifestSchema(StrBuilder* out) {
    out->Append(StrL(
        "{\"$schema\":\"https://json-schema.org/draft/2020-12/schema\","
        "\"title\":\"gpui-shell application manifest\",\"type\":\"object\","
        "\"additionalProperties\":false,\"required\":[\"id\",\"name\",\"entry\"],"
        "\"properties\":{\"id\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"},"
        "\"version\":{\"type\":\"string\"},\"shell-version\":{\"type\":\"string\"},"
        "\"entry\":{\"type\":\"string\"},\"capabilities\":{\"type\":\"object\"}}}"));
}

static bool AbsolutePath(Str path) {
    if (!path) return false;
    if (path.s[0] == '/' || path.s[0] == '\\') return true;
    return path.len >= 3 && path.s[1] == ':' &&
           (path.s[2] == '/' || path.s[2] == '\\');
}

static Str ExpandPath(Str raw, Str plugin, Str data) {
    StrBuilder out;
    for (int i = 0; i < raw.len;) {
        if (i + 12 <= raw.len &&
            StrEq(Str(raw.s + i, 12), "${pluginDir}")) {
            out.Append(plugin);
            i += 12;
        } else if (i + 10 <= raw.len &&
                   StrEq(Str(raw.s + i, 10), "${dataDir}")) {
            out.Append(data);
            i += 10;
        } else {
            out.AppendChar(raw.s[i++]);
        }
    }
    Str expanded = out.TakeStr();
    if (AbsolutePath(expanded)) return expanded;
    StrBuilder joined;
    joined.Append(plugin);
    if (plugin && plugin.s[plugin.len - 1] != '/' &&
        plugin.s[plugin.len - 1] != '\\')
        joined.AppendChar(GPUI_OS_WINDOWS ? '\\' : '/');
    joined.Append(expanded);
    StrFree(expanded);
    return joined.TakeStr();
}

Capabilities PluginManifest::Grant(Str pluginDirectory,
                                   Str dataDirectory) const {
    Capabilities result;
    for (int i = 0; i < readRoots.len; i++) {
        Str path = ExpandPath(readRoots[i], pluginDirectory, dataDirectory);
        result.AddReadRoot(path);
        StrFree(path);
    }
    for (int i = 0; i < writeRoots.len; i++) {
        Str path = ExpandPath(writeRoots[i], pluginDirectory, dataDirectory);
        result.AddWriteRoot(path);
        StrFree(path);
    }
    if (executeUnrestricted) {
        result.SetExecute(ExecuteGrant::Unrestricted());
    } else if (execute.len > 0) {
        ExecuteGrant grant = ExecuteGrant::Allowed(execute.els, execute.len);
        result.SetExecute(grant);
    }
    for (int i = 0; i < networkHosts.len; i++)
        result.AddNetworkHost(networkHosts[i]);
    for (int i = 0; i < http.len; i++) {
        PluginHttpGrant* file = http[i];
        HttpRequestGrant grant(file->host);
        grant.Scheme(file->scheme);
        if (file->hasPort) grant.Port(file->port);
        for (int j = 0; j < file->methods.len; j++)
            grant.AddMethod(file->methods[j]);
        for (int j = 0; j < file->paths.len; j++)
            grant.AddPath(file->paths[j]);
        for (int j = 0; j < file->pathPrefixes.len; j++)
            grant.AddPathPrefix(file->pathPrefixes[j]);
        result.AddHttpRequest(grant);
    }
    return result.Storage(storage)
        .ClipboardRead(clipboardRead)
        .ClipboardWrite(clipboardWrite)
        .Exit(exit);
}

static int ComparePaths(const void* left, const void* right) {
    const Str* a = (const Str*)left;
    const Str* b = (const Str*)right;
    int n = a->len < b->len ? a->len : b->len;
    int compared = n ? memcmp(a->s, b->s, (size_t)n) : 0;
    return compared ? compared : a->len - b->len;
}

static Str DefaultDataHome() {
    const char* explicitHome = getenv("XDG_DATA_HOME");
    if (explicitHome && *explicitHome) return StrDup(Str(explicitHome));
#if GPUI_OS_WINDOWS
    const char* appData = getenv("APPDATA");
    if (appData && *appData) return StrDup(Str(appData));
#endif
    const char* user = getenv(GPUI_OS_WINDOWS ? "USERPROFILE" : "HOME");
    char cwd[kMaxPath] = {};
    if (!user || !*user) {
        PlatGetCwd(cwd, kMaxPath);
        user = cwd;
    }
    StrBuilder path;
    path.Append(Str(user));
#if GPUI_OS_MAC
    path.Append(StrL("/Library/Application Support"));
#elif GPUI_OS_WINDOWS
    path.Append(StrL("\\AppData\\Roaming"));
#else
    path.Append(StrL("/.local/share"));
#endif
    return path.TakeStr();
}

PluginManager::PluginManager() : dataHome(DefaultDataHome()) {}
PluginManager::PluginManager(Str directory) : PluginManager() {
    AddDirectory(directory);
}

static void FreePlugin(Plugin* plugin, App* app) {
    if (!plugin) return;
    ScriptView* view = plugin->view.Get(app);
    if (view && view->object)
        plugin->runtime->ReleaseApplicationState(view->object);
    if (plugin->view.IsValid()) EntityDrop(app, plugin->view.id);
    delete plugin->assets;
    PolicyClearHostModules(plugin->policy);
    PolicyRelease(plugin->policy);
    plugin->runtime->Release();
    StrFree(plugin->root);
    StrFree(plugin->dataDirectory);
    StrFree(plugin->storePath);
    delete plugin;
}

PluginManager::~PluginManager() {
    for (int i = 0; i < loaded.len; i++)
        FreePlugin(loaded[i], loaded[i]->app);
    loaded.Reset();
    ClearCatalog();
    for (int i = 0; i < directories.len; i++) StrFree(directories[i]);
    directories.Reset();
    StrFree(dataHome);
}

PluginManager& PluginManager::AddDirectory(Str directory) {
    directories.Append(StrDup(directory));
    return *this;
}

PluginManager& PluginManager::DataHome(Str directory) {
    StrFree(dataHome);
    dataHome = StrDup(directory);
    return *this;
}

void PluginManager::ClearCatalog() {
    for (int i = 0; i < catalog.len; i++) {
        delete catalog[i].manifest;
        StrFree(catalog[i].root);
        StrFree(catalog[i].error);
    }
    catalog.Reset();
}

static bool ManifestAt(Str root) {
    Arena* arena = ArenaNew();
    Str manifest = Join(arena, root, Str(kShellManifestFile));
    bool found = false;
    if (manifest.len < kMaxPath) {
        char path[kMaxPath];
        memcpy(path, manifest.s, (size_t)manifest.len);
        path[manifest.len] = 0;
        found = PlatFileExists(path);
    }
    ArenaDelete(arena);
    return found;
}

const Vec<PluginDiscovery>& PluginManager::Discover() {
    ClearCatalog();
    discovered = true;
    Vec<Str> roots;
    for (int d = 0; d < directories.len; d++) {
        if (ManifestAt(directories[d])) {
            roots.Append(StrDup(directories[d]));
            continue;
        }
        if (directories[d].len >= kMaxPath) continue;
        char directory[kMaxPath];
        memcpy(directory, directories[d].s, (size_t)directories[d].len);
        directory[directories[d].len] = 0;
        DirEntry* entries = AllocArray<DirEntry>(4096);
        int count = entries ? PlatListDir(directory, entries, 4096) : 0;
        Vec<Str> directoryRoots;
        for (int i = 0; i < count; i++) {
            if (!entries[i].isDir || entries[i].isSymlink) continue;
            Arena* scratch = ArenaNew();
            Str root = Join(scratch, directories[d], Str(entries[i].name));
            if (ManifestAt(root)) directoryRoots.Append(StrDup(root));
            ArenaDelete(scratch);
        }
        free(entries);
        if (directoryRoots.len > 1)
            qsort(directoryRoots.els, (size_t)directoryRoots.len, sizeof(Str),
                  ComparePaths);
        for (int i = 0; i < directoryRoots.len; i++) {
            roots.Append(directoryRoots[i]);
            directoryRoots[i] = {};
        }
        directoryRoots.Reset();
    }
    for (int i = 0; i < roots.len; i++) {
        PluginDiscovery found;
        found.root = roots[i];
        roots[i] = {};
        auto* manifest = new PluginManifest();
        ShellError error = {};
        if (!PluginManifestRead(found.root, manifest, &error)) {
            found.error = error.message;
            error.message = {};
            delete manifest;
        } else {
            for (int j = 0; j < catalog.len; j++) {
                if (catalog[j].manifest &&
                    StrEq(catalog[j].manifest->id, manifest->id)) {
                    found.error = StrDup(fmt("`%s` is already provided by %s", manifest->id, catalog[j].root));
                    delete manifest;
                    manifest = nullptr;
                    break;
                }
            }
            found.manifest = manifest;
        }
        catalog.Append(found);
        ShellErrorClear(&error);
    }
    roots.Reset();
    return catalog;
}

Str PluginManager::DataDirectory(Str id, Arena* arena) const {
    Str first = Join(arena, dataHome, StrL("gpui-shell"));
    Str second = Join(arena, first, StrL("plugins"));
    return Join(arena, second, id);
}

bool PluginManager::Load(ShellRuntime* runtime, Str id,
                         PluginAuthorizeFn authorize, void* authorizeData,
                         Window* window, App* app, ShellError* error) {
    ShellErrorClear(error);
    if (!discovered) {
        SetError(error, StrL("plugin discovery has not run; call Discover first"));
        return false;
    }
    if (Loaded(id)) {
        SetError(error, fmt("plugin `%s` is already loaded", id));
        return false;
    }
    const PluginDiscovery* selected = nullptr;
    for (int i = 0; i < catalog.len; i++)
        if (catalog[i].manifest && StrEq(catalog[i].manifest->id, id))
            selected = &catalog[i];
    if (!selected) {
        SetError(error, fmt("no plugin `%s`", id));
        return false;
    }
    if (authorize && !authorize(selected->manifest, authorizeData)) {
        SetError(error, fmt("capabilities for plugin `%s` were not approved", id));
        return false;
    }
    Arena* scratch = ArenaNew();
    Str data = DataDirectory(id, scratch);
    FsResult mkdirResult;
    Str mkdirError;
    if (dataHome) {
        Str relative = Join(scratch, StrL("gpui-shell/plugins"), id);
        FsRun(FsOperation::MakeDirectory, dataHome, relative, {}, true,
              &mkdirResult, &mkdirError);
        mkdirResult.Free();
        StrFree(mkdirError);
    }
    Capabilities capabilities = selected->manifest->Grant(selected->root, data);
    Policy* policy = PolicyNew(capabilities);
    Str store = Join(scratch, data, StrL("store.json"));
    if (capabilities.HasStorage()) {
        Str storageError;
        if (!PolicySetStoragePath(policy, store, &storageError)) {
            log(fmt("storage is unavailable for `%s`: %s", id,
                    storageError));
            StrFree(storageError);
        }
    }
    ViewType* type = runtime->LoadApp(selected->root,
                                      selected->manifest->entry, policy, error);
    if (!type) {
        PolicyRelease(policy);
        ArenaDelete(scratch);
        return false;
    }
    Entity<ScriptView> view = ScriptView::New(app, runtime, type, policy);
    ViewTypeRelease(type);
    ScriptView* state = view.Get(app);
    state->object = runtime->Instantiate(state->type, window, app, policy,
                                         error, view.id);
    if (!state->object) {
        EntityDrop(app, view.id);
        PolicyRelease(policy);
        ArenaDelete(scratch);
        return false;
    }
    auto* plugin = new Plugin();
    plugin->manifest = selected->manifest;
    plugin->root = StrDup(selected->root);
    plugin->dataDirectory = StrDup(data);
    plugin->storePath = StrDup(store);
    plugin->policy = policy;
    plugin->runtime = runtime->Retain();
    plugin->view = view;
    plugin->assets = new AppAssets(selected->root);
    plugin->assets->Install();
    plugin->app = app;
    loaded.Append(plugin);
    ArenaDelete(scratch);
    return true;
}

bool PluginManager::Unload(Str id, App* app) {
    for (int i = 0; i < loaded.len; i++) {
        if (!loaded[i]->manifest || !StrEq(loaded[i]->manifest->id, id))
            continue;
        Plugin* plugin = loaded[i];
        for (int j = i + 1; j < loaded.len; j++) loaded[j - 1] = loaded[j];
        loaded.len--;
        FreePlugin(plugin, app);
        return true;
    }
    return false;
}

const Plugin* PluginManager::Loaded(Str id) const {
    for (int i = 0; i < loaded.len; i++)
        if (loaded[i]->manifest && StrEq(loaded[i]->manifest->id, id))
            return loaded[i];
    return nullptr;
}

Entity<ShellRoot> ShellLoadApplication(ShellRuntime* runtime, Str directory,
                                       Window* window, App* app,
                                       Policy* policy, ShellError* error,
                                       Str* resolvedEntry) {
    ShellErrorClear(error);
    Str entry = StrL("main.js");
    PluginManifest manifest;
    if (ManifestAt(directory)) {
        if (!PluginManifestRead(directory, &manifest, error)) return {};
        entry = manifest.entry;
    }
    if (resolvedEntry) *resolvedEntry = StrDup(entry);
    Policy* authority = policy ? PolicyRetain(policy) : PolicyDefault();
    ViewType* type = runtime->LoadApp(directory, entry, authority, error);
    if (!type) {
        PolicyRelease(authority);
        return {};
    }
    Entity<ScriptView> view = ScriptView::New(app, runtime, type, authority);
    ViewTypeRelease(type);
    ScriptView* state = view.Get(app);
    state->object = runtime->Instantiate(state->type, window, app, authority,
                                         error, view.id);
    PolicyRelease(authority);
    if (!state->object) {
        EntityDrop(app, view.id);
        return {};
    }
    return ShellRoot::New(app, view.id);
}

Str ShellCheckApplication(Arena* arena, ShellRuntime* runtime, Str directory,
                          Window* window, App* app, Policy* policy,
                          ShellError* error) {
    Entity<ShellRoot> root = ShellLoadApplication(runtime, directory, window,
                                                   app, policy, error);
    if (!root.IsValid()) return {};
    ShellRoot* shellRoot = root.Get(app);
    ScriptView* view = shellRoot && shellRoot->content.IsValid()
                           ? Entity<ScriptView>{shellRoot->content}.Get(app)
                           : nullptr;
    Str result = view && view->object
                     ? runtime->RenderToSpec(arena, view->object, window, app,
                                             view->self, view->policy, error)
                     : Str{};
    EntityDrop(app, root.id);
    return result;
}

} // namespace gpui::shell
