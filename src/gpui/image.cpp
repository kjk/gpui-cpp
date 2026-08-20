#include "gpui/image.h"

#include "gpui/assets.h"
#include "gpui/paint.h"

namespace gpui {

// ─── data: URIs ───────────────────────────────────────────────────────────

static int Base64Value(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '+') {
        return 62;
    }
    if (c == '/') {
        return 63;
    }
    return -1;
}

// Decodes into `out`, ignoring whatever is not base64 — a data: URI wrapped
// across lines in a document is still one payload.
static void Base64Decode(Str s, Vec<uint8_t>* out) {
    uint32_t acc = 0;
    int bits = 0;
    for (int i = 0; i < s.len; i++) {
        int v = Base64Value(s.s[i]);
        if (v < 0) {
            continue;
        }
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out->Append((uint8_t)((acc >> bits) & 0xff));
        }
    }
}

static int HexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static void PercentDecode(Str s, Vec<uint8_t>* out) {
    for (int i = 0; i < s.len; i++) {
        if (s.s[i] == '%' && i + 2 < s.len) {
            int hi = HexValue(s.s[i + 1]);
            int lo = HexValue(s.s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out->Append((uint8_t)(hi * 16 + lo));
                i += 2;
                continue;
            }
        }
        out->Append((uint8_t)s.s[i]);
    }
}

static bool StartsWithNoCase(Str s, const char* prefix) {
    int n = (int)strlen(prefix);
    if (s.len < n) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        char c = s.s[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != prefix[i]) {
            return false;
        }
    }
    return true;
}

// "data:image/png;base64,iVBOR..." — the payload after the comma, decoded by
// whichever of the two encodings the header names.
static bool DataUriBytes(Str src, Vec<uint8_t>* out) {
    if (!StartsWithNoCase(src, "data:")) {
        return false;
    }
    int comma = -1;
    for (int i = 5; i < src.len; i++) {
        if (src.s[i] == ',') {
            comma = i;
            break;
        }
    }
    if (comma < 0) {
        return false;
    }
    Str header(src.s + 5, comma - 5);
    Str payload(src.s + comma + 1, src.len - comma - 1);
    bool base64 = false;
    for (int i = 0; i + 6 <= header.len; i++) {
        if (memcmp(header.s + i, "base64", 6) == 0) {
            base64 = true;
            break;
        }
    }
    if (base64) {
        Base64Decode(payload, out);
    } else {
        PercentDecode(payload, out);
    }
    return out->len > 0;
}

bool ImageSrcIsLocal(Str src) {
    if (!src.s || src.len <= 0) {
        return false;
    }
    if (StartsWithNoCase(src, "data:")) {
        return true;
    }
    // Anything with a scheme is somewhere else: http, https, ftp, mailto.
    for (int i = 0; i + 2 < src.len; i++) {
        if (src.s[i] == ':' && src.s[i + 1] == '/' && src.s[i + 2] == '/') {
            return false;
        }
    }
    return true;
}

// ─── the cache ────────────────────────────────────────────────────────────
//
// A document shows the same badge or logo more than once and repaints many
// times a second, so a decode has to happen once. The key is the src as
// written; a failed decode is remembered too, or a document full of remote
// images would retry every one of them every frame.

struct ImageCacheSlot {
    Str src = {};
    Image* img = nullptr;
    bool tried = false;
};

// A page shows a handful; past that the oldest slot is reused.
constexpr int kImageCacheSlots = 32;

static ImageCacheSlot gImageCache[kImageCacheSlots];
static int gImageCacheNext = 0;

static bool SrcEq(Str a, Str b) {
    return a.len == b.len && (a.len == 0 || memcmp(a.s, b.s, (size_t)a.len) == 0);
}

static void ImageSlotFree(ImageCacheSlot* s) {
    if (s->img) {
        ImageFree(s->img);
        s->img = nullptr;
    }
    if (s->src.s) {
        StrFree(s->src);
        s->src = {};
    }
    s->tried = false;
}

void ImageCacheClear() {
    for (int i = 0; i < kImageCacheSlots; i++) {
        ImageSlotFree(&gImageCache[i]);
    }
    gImageCacheNext = 0;
}

Image* ImageForSrc(PaintApp* pa, Str src) {
    if (!pa || !src.s || src.len <= 0) {
        return nullptr;
    }
    for (int i = 0; i < kImageCacheSlots; i++) {
        if (gImageCache[i].tried && SrcEq(gImageCache[i].src, src)) {
            return gImageCache[i].img;
        }
    }
    if (!ImageSrcIsLocal(src)) {
        return nullptr;
    }

    Vec<uint8_t> bytes;
    bool have = DataUriBytes(src, &bytes);
    if (!have) {
        have = AssetsLoad(src, &bytes);
    }
    Image* img =
        have && bytes.len > 0 ? ImageDecode(pa, bytes.els, bytes.len) : nullptr;

    ImageCacheSlot* slot = &gImageCache[gImageCacheNext];
    gImageCacheNext = (gImageCacheNext + 1) % kImageCacheSlots;
    ImageSlotFree(slot);
    slot->src = StrDup(src);
    slot->img = img;
    slot->tried = true;
    return img;
}

} // namespace gpui
