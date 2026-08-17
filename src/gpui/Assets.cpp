#include "gpui/Assets.h"

#include "Base.h"

namespace gpui {

static const int kMaxRoots = 12;
static char gRoots[kMaxRoots][MAX_PATH];
static int gRootN = 0;

void AssetsClear() {
    gRootN = 0;
}

static void AddRootRaw(const char* dir) {
    if (!dir || !dir[0] || gRootN >= kMaxRoots) {
        return;
    }
    for (int i = 0; i < gRootN; i++) {
        if (_stricmp(gRoots[i], dir) == 0) {
            return;
        }
    }
    DWORD attr = GetFileAttributesA(dir);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return;
    }
    strncpy_s(gRoots[gRootN], MAX_PATH, dir, _TRUNCATE);
    gRootN++;
}

void AssetsAddRoot(Str dir) {
    if (!dir.s || dir.len <= 0) {
        return;
    }
    char buf[MAX_PATH];
    int n = dir.len < MAX_PATH - 1 ? dir.len : MAX_PATH - 1;
    memcpy(buf, dir.s, (size_t)n);
    buf[n] = 0;
    AddRootRaw(buf);
}

static void JoinPath(char* dst, int dstN, const char* a, const char* b) {
    if (!a || !a[0]) {
        strncpy_s(dst, (size_t)dstN, b ? b : "", _TRUNCATE);
        return;
    }
    if (!b || !b[0]) {
        strncpy_s(dst, (size_t)dstN, a, _TRUNCATE);
        return;
    }
    _snprintf_s(dst, (size_t)dstN, _TRUNCATE, "%s\\%s", a, b);
}

static void ParentDir(char* path) {
    int n = (int)strlen(path);
    while (n > 0 && (path[n - 1] == '\\' || path[n - 1] == '/')) {
        path[--n] = 0;
    }
    while (n > 0 && path[n - 1] != '\\' && path[n - 1] != '/') {
        path[--n] = 0;
    }
    while (n > 0 && (path[n - 1] == '\\' || path[n - 1] == '/')) {
        path[--n] = 0;
    }
}

static void SlashToBack(char* s) {
    for (; *s; s++) {
        if (*s == '/') {
            *s = '\\';
        }
    }
}

void AssetsAddDefaultRoots(Str exampleName) {
    char cwd[MAX_PATH] = {};
    GetCurrentDirectoryA(MAX_PATH, cwd);

    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    ParentDir(exe);

    char sub[MAX_PATH];
    if (exampleName.s && exampleName.len > 0) {
        _snprintf_s(sub, MAX_PATH, _TRUNCATE, "assets\\%s", exampleName.s);
    } else {
        strncpy_s(sub, MAX_PATH, "assets", _TRUNCATE);
    }

    char p[MAX_PATH];
    JoinPath(p, MAX_PATH, cwd, sub);
    AddRootRaw(p);
    JoinPath(p, MAX_PATH, exe, sub);
    AddRootRaw(p);

    // Walk parents of cwd and exe looking for assets/<name>
    char walk[MAX_PATH];
    for (int src = 0; src < 2; src++) {
        strncpy_s(walk, MAX_PATH, src == 0 ? cwd : exe, _TRUNCATE);
        for (int up = 0; up < 6; up++) {
            JoinPath(p, MAX_PATH, walk, sub);
            AddRootRaw(p);
            if (exampleName.s) {
                // rust layout: examples/app_assets/assets
                char rust[MAX_PATH];
                _snprintf_s(rust, MAX_PATH, _TRUNCATE, "examples\\%s\\assets",
                            exampleName.s);
                JoinPath(p, MAX_PATH, walk, rust);
                AddRootRaw(p);
                _snprintf_s(rust, MAX_PATH, _TRUNCATE,
                            ".work\\gpui-component\\examples\\%s\\assets",
                            exampleName.s);
                JoinPath(p, MAX_PATH, walk, rust);
                AddRootRaw(p);
            }
            char prev[MAX_PATH];
            strncpy_s(prev, MAX_PATH, walk, _TRUNCATE);
            ParentDir(walk);
            if (!walk[0] || _stricmp(prev, walk) == 0) {
                break;
            }
        }
    }
}

static bool ReadFileAll(const char* path, Vec<u8>* out) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart < 0 ||
        sz.QuadPart > 8 * 1024 * 1024) {
        CloseHandle(h);
        return false;
    }
    int n = (int)sz.QuadPart;
    out->Reset();
    if (n > 0) {
        u8* buf = out->AppendBlanks(n);
        if (!buf) {
            CloseHandle(h);
            return false;
        }
        DWORD got = 0;
        BOOL ok = ReadFile(h, buf, (DWORD)n, &got, nullptr);
        CloseHandle(h);
        if (!ok || (int)got != n) {
            out->Reset();
            return false;
        }
        return true;
    }
    CloseHandle(h);
    return true;
}

bool AssetsLoad(Str relPath, Vec<u8>* out) {
    if (!relPath.s || relPath.len <= 0 || !out) {
        return false;
    }
    char rel[MAX_PATH];
    int n = relPath.len < MAX_PATH - 1 ? relPath.len : MAX_PATH - 1;
    memcpy(rel, relPath.s, (size_t)n);
    rel[n] = 0;
    SlashToBack(rel);

    for (int i = 0; i < gRootN; i++) {
        char full[MAX_PATH];
        JoinPath(full, MAX_PATH, gRoots[i], rel);
        if (ReadFileAll(full, out)) {
            return true;
        }
    }
    return false;
}

TempStr AssetsLoadTextTemp(Str relPath) {
    Vec<u8> buf;
    if (!AssetsLoad(relPath, &buf) || buf.len <= 0) {
        return {};
    }
    Str s = AllocStrTemp(buf.len);
    if (!s.s) {
        return {};
    }
    memcpy(s.s, buf.els, (size_t)buf.len);
    s.s[buf.len] = 0;
    return s;
}

bool AssetsExists(Str relPath) {
    Vec<u8> buf;
    return AssetsLoad(relPath, &buf);
}
} // namespace gpui
