#include "gpui/assets.h"

#include "base.h"

#include <stdio.h>

namespace gpui {

#if GPUI_OS_WINDOWS
static const char kSep = '\\';
#else
static const char kSep = '/';
#endif

static const int kMaxRoots = 12;
static char gRoots[kMaxRoots][kMaxPath];
static int gRootN = 0;

struct RegisteredAssetSource {
    int id = 0;
    void* user = nullptr;
    AssetLoadFn load = nullptr;
    AssetExistsFn exists = nullptr;
};

static RegisteredAssetSource gSources[kMaxRoots];
static int gSourceN = 0;
static int gNextSource = 1;

void AssetsClear() {
    gRootN = 0;
    gSourceN = 0;
}

static void AddRootRaw(const char* dir) {
    if (!dir || !dir[0] || gRootN >= kMaxRoots) {
        return;
    }
    for (int i = 0; i < gRootN; i++) {
        if (base::StrEqI(Str(gRoots[i]), dir)) {
            return;
        }
    }
    if (!PlatDirExists(dir)) {
        return;
    }
    StrCopyZ(gRoots[gRootN], kMaxPath, dir);
    gRootN++;
}

// How many roots are registered, which is what AppNew asks before it
// supplies a default.
int AssetsRootCount() {
    return gRootN + gSourceN;
}

int AssetsAddSource(void* user, AssetLoadFn load, AssetExistsFn exists) {
    if (!user || !load || gSourceN >= kMaxRoots) return 0;
    int id = gNextSource++;
    if (id <= 0) {
        gNextSource = 2;
        id = 1;
    }
    gSources[gSourceN++] = {id, user, load, exists};
    return id;
}

void AssetsRemoveSource(int id) {
    for (int i = 0; i < gSourceN; i++) {
        if (gSources[i].id != id) continue;
        for (int j = i + 1; j < gSourceN; j++)
            gSources[j - 1] = gSources[j];
        gSourceN--;
        return;
    }
}

void AssetsAddRoot(Str dir) {
    if (!dir.s || dir.len <= 0) {
        return;
    }
    char buf[kMaxPath];
    int n = dir.len < kMaxPath - 1 ? dir.len : kMaxPath - 1;
    memcpy(buf, dir.s, (size_t)n);
    buf[n] = 0;
    AddRootRaw(buf);
}

static void JoinPath(char* dst, int dstN, const char* a, const char* b) {
    if (!a || !a[0]) {
        StrCopyZ(dst, dstN, b ? b : "");
        return;
    }
    if (!b || !b[0]) {
        StrCopyZ(dst, dstN, a);
        return;
    }
    // Copy and append rather than snprintf: truncating to dstN is the whole
    // point here, and gcc's -Wformat-truncation cannot tell that from a bug
    // once it inlines this into a caller whose buffer it can size.
    StrCopyZ(dst, dstN, a);
    int n = (int)strlen(dst);
    if (n + 1 < dstN) {
        dst[n++] = kSep;
        dst[n] = 0;
        StrCopyZ(dst + n, dstN - n, b);
    }
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

// Asset paths are written with forward slashes; rewrite them to whatever the
// OS wants.
static void ToNativeSep(char* s) {
    for (; *s; s++) {
        if (*s == '/' || *s == '\\') {
            *s = kSep;
        }
    }
}

void AssetsAddDefaultRoots(Str exampleName) {
    char cwd[kMaxPath] = {};
    PlatGetCwd(cwd, kMaxPath);

    char exe[kMaxPath] = {};
    PlatGetExeDir(exe, kMaxPath);

    char sub[kMaxPath];
    if (exampleName.s && exampleName.len > 0) {
        snprintf(sub, kMaxPath, "assets%c%s", kSep, exampleName.s);
    } else {
        StrCopyZ(sub, kMaxPath, "assets");
    }

    char p[kMaxPath];
    JoinPath(p, kMaxPath, cwd, sub);
    AddRootRaw(p);
    JoinPath(p, kMaxPath, exe, sub);
    AddRootRaw(p);

    // Walk parents of cwd and exe looking for assets/<name>
    char walk[kMaxPath];
    char work[kMaxPath];
    for (int src = 0; src < 2; src++) {
        StrCopyZ(walk, kMaxPath, src == 0 ? cwd : exe);
        for (int up = 0; up < 6; up++) {
            JoinPath(p, kMaxPath, walk, sub);
            AddRootRaw(p);
            if (exampleName.s) {
                // rust layout: examples/app_assets/assets
                char rust[kMaxPath];
                snprintf(rust, kMaxPath, "examples%c%s%cassets", kSep,
                         exampleName.s, kSep);
                JoinPath(p, kMaxPath, walk, rust);
                AddRootRaw(p);
                snprintf(rust, kMaxPath,
                         ".work%cgpui-component%cexamples%c%s%cassets", kSep,
                         kSep, kSep, exampleName.s, kSep);
                JoinPath(p, kMaxPath, walk, rust);
                AddRootRaw(p);
            }
            // The pinned Rust clone itself, which is where a folder that
            // belongs to no one example lives — `themes/`, the theme files
            // the registry reads.
            snprintf(work, kMaxPath, ".work%cgpui-component", kSep);
            JoinPath(p, kMaxPath, walk, work);
            AddRootRaw(p);
            char prev[kMaxPath];
            StrCopyZ(prev, kMaxPath, walk);
            ParentDir(walk);
            if (!walk[0] || base::StrEqI(Str(prev), walk)) {
                break;
            }
        }
    }
}

static bool ReadFileAll(const char* path, Vec<uint8_t>* out) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long size = ftell(f);
    if (size < 0 || size > 8 * 1024 * 1024) {
        fclose(f);
        return false;
    }
    rewind(f);
    VecReset(*out);
    int n = (int)size;
    if (n == 0) {
        fclose(f);
        return true;
    }
    uint8_t* buf = VecAppendBlanks(*out, n);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    if ((int)got != n) {
        VecReset(*out);
        return false;
    }
    return true;
}

bool AssetsFindDir(Str relDir, char* out, int cap) {
    if (!relDir.s || relDir.len <= 0 || !out || cap <= 0) {
        return false;
    }
    char rel[kMaxPath];
    int n = relDir.len < kMaxPath - 1 ? relDir.len : kMaxPath - 1;
    memcpy(rel, relDir.s, (size_t)n);
    rel[n] = 0;
    ToNativeSep(rel);

    for (int i = 0; i < gRootN; i++) {
        char full[kMaxPath];
        JoinPath(full, kMaxPath, gRoots[i], rel);
        if (PlatDirExists(full)) {
            StrCopyZ(out, cap, full);
            return true;
        }
    }
    return false;
}

bool AssetsLoad(Str relPath, Vec<uint8_t>* out) {
    if (!relPath.s || relPath.len <= 0 || !out) {
        return false;
    }
    for (int i = gSourceN - 1; i >= 0; i--)
        if (gSources[i].load(gSources[i].user, relPath, out)) return true;

    char rel[kMaxPath];
    int n = relPath.len < kMaxPath - 1 ? relPath.len : kMaxPath - 1;
    memcpy(rel, relPath.s, (size_t)n);
    rel[n] = 0;
    ToNativeSep(rel);

    for (int i = 0; i < gRootN; i++) {
        char full[kMaxPath];
        JoinPath(full, kMaxPath, gRoots[i], rel);
        if (ReadFileAll(full, out)) {
            return true;
        }
    }
    return false;
}

TempStr AssetsLoadTextTemp(Str relPath) {
    Vec<uint8_t> buf;
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

// Whether a root has the file, asked without reading it. This used to be
// AssetsLoad into a throwaway buffer, which meant an fopen and a full read
// per candidate path; the image layout asks it several times per picture per
// measure pass, so it was the single biggest thing in the story's layout.
bool AssetsExists(Str relPath) {
    if (!relPath.s || relPath.len <= 0) {
        return false;
    }
    for (int i = gSourceN - 1; i >= 0; i--)
        if (gSources[i].exists &&
            gSources[i].exists(gSources[i].user, relPath))
            return true;

    char rel[kMaxPath];
    int n = relPath.len < kMaxPath - 1 ? relPath.len : kMaxPath - 1;
    memcpy(rel, relPath.s, (size_t)n);
    rel[n] = 0;
    ToNativeSep(rel);

    for (int i = 0; i < gRootN; i++) {
        char full[kMaxPath];
        JoinPath(full, kMaxPath, gRoots[i], rel);
        if (PlatFileExists(full)) {
            return true;
        }
    }
    return false;
}
} // namespace gpui
