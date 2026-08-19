/* Port of crates/base/src/input/base/mask_pattern.rs.

   Rust parses the pattern once into a `Vec<MaskToken>` and keeps it beside the
   pattern string. A token is a pure function of its character, so here the
   pattern string is the whole state and MaskTokenAt reads it — the patterns
   are a dozen characters long and every walk over them is already a walk over
   the text beside it.

   Rust indexes both the pattern and the text by *character*, not by byte, so
   everything below steps codepoints. */

#include "gpui/Gpui.h"

namespace gpui {

static bool IsAsciiDigit(uint32_t c) {
    return c >= '0' && c <= '9';
}
static bool IsAsciiAlpha(uint32_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static bool IsAsciiAlnum(uint32_t c) {
    return IsAsciiDigit(c) || IsAsciiAlpha(c);
}
static bool IsSign(uint32_t c) {
    return c == '+' || c == '-';
}

// MaskToken::is_match. A separator matches only itself.
static bool TokenIsMatch(MaskToken tok, uint32_t sep, uint32_t ch) {
    switch (tok) {
        case MaskToken::Digit:
            return IsAsciiDigit(ch);
        case MaskToken::Letter:
            return IsAsciiAlpha(ch);
        case MaskToken::LetterOrDigit:
            return IsAsciiAlnum(ch);
        case MaskToken::Any:
            return true;
        case MaskToken::Sep:
            return sep == ch;
    }
    return false;
}

// MaskToken::mask_char.
static uint32_t TokenMaskChar(MaskToken tok, uint32_t sep, uint32_t ch) {
    return tok == MaskToken::Sep ? sep : ch;
}

// MaskToken::unmask_char. A separator contributes nothing — Rust's `None`.
static bool TokenUnmaskChar(MaskToken tok) {
    return tok != MaskToken::Sep;
}

static MaskToken TokenOf(uint32_t ch, uint32_t* sep) {
    *sep = 0;
    switch (ch) {
        case '9':
            return MaskToken::Digit;
        case 'A':
            return MaskToken::Letter;
        case '#':
            return MaskToken::LetterOrDigit;
        case '*':
            return MaskToken::Any;
        default:
            *sep = ch;
            return MaskToken::Sep;
    }
}

MaskPattern MaskPatternNew(Str pattern) {
    MaskPattern p = {};
    p.kind = MaskKind::Pattern;
    p.pattern = StrDup(pattern);
    return p;
}

MaskPattern MaskPatternNumber(uint32_t separator) {
    MaskPattern p = {};
    p.kind = MaskKind::Number;
    p.separator = separator;
    p.fraction = -1;
    return p;
}

void MaskPatternFree(MaskPattern* p) {
    if (!p) {
        return;
    }
    StrFree(p->pattern);
    p->pattern = {};
    p->kind = MaskKind::None;
}

bool MaskTokenAt(const MaskPattern& p, int pos, MaskToken* out, uint32_t* sep) {
    *out = MaskToken::Any;
    *sep = 0;
    if (p.kind != MaskKind::Pattern || pos < 0) {
        return false;
    }
    int i = RopeCharIndexToOffset(p.pattern, pos);
    uint32_t ch = 0;
    if (RopeCharAt(p.pattern, i, &ch) == 0) {
        return false;
    }
    *out = TokenOf(ch, sep);
    return true;
}

bool MaskIsNone(const MaskPattern& p) {
    switch (p.kind) {
        case MaskKind::Pattern:
            return p.pattern.len == 0;
        case MaskKind::Number:
            return false;
        case MaskKind::None:
            return true;
    }
    return true;
}

// The number half of is_valid: at most one dot, at most one sign and only at
// the front, digits or the group separator everywhere else.
static bool NumberIsValid(const MaskPattern& p, Str text) {
    if (text.len == 0) {
        return true;
    }
    int dot = -1;
    for (int i = 0; i < text.len; i++) {
        if (text.s[i] != '.') {
            continue;
        }
        if (dot >= 0) {
            return false; // only one dot is valid
        }
        dot = i;
    }
    int intEnd = dot < 0 ? text.len : dot;
    int charPos = 0;
    for (int i = 0; i < intEnd;) {
        uint32_t c = 0;
        i += Utf8At(text, i, &c);
        if (IsSign(c)) {
            // Only one sign, and only at the beginning of the string.
            if (charPos != 0) {
                return false;
            }
        } else if (!IsAsciiDigit(c) && !(p.separator && c == p.separator)) {
            return false;
        }
        charPos++;
    }
    for (int i = intEnd + 1; i < text.len && dot >= 0;) {
        uint32_t c = 0;
        i += Utf8At(text, i, &c);
        if (!IsAsciiDigit(c) && !(p.separator && c == p.separator)) {
            return false;
        }
    }
    return true;
}

bool MaskIsValid(const MaskPattern& p, Str maskText) {
    if (MaskIsNone(p)) {
        return true;
    }
    if (p.kind == MaskKind::Number) {
        return NumberIsValid(p, maskText);
    }
    // Rust walks the tokens, consuming a text character for each one that
    // matches, and calls the text valid when every character was consumed.
    int ti = 0;
    int tokens = RopeOffsetToCharIndex(p.pattern, p.pattern.len);
    for (int pos = 0; pos < tokens; pos++) {
        if (ti >= maskText.len) {
            break;
        }
        MaskToken tok = MaskToken::Any;
        uint32_t sep = 0;
        MaskTokenAt(p, pos, &tok, &sep);
        uint32_t ch = 0;
        int n = Utf8At(maskText, ti, &ch);
        if (TokenIsMatch(tok, sep, ch)) {
            ti += n;
        }
    }
    return ti == maskText.len;
}

bool MaskIsValidAt(const MaskPattern& p, uint32_t ch, int pos) {
    if (MaskIsNone(p) || p.kind != MaskKind::Pattern) {
        return true;
    }
    MaskToken tok = MaskToken::Any;
    uint32_t sep = 0;
    if (!MaskTokenAt(p, pos, &tok, &sep)) {
        return false;
    }
    if (TokenIsMatch(tok, sep, ch)) {
        return true;
    }
    // A separator is skipped over: if the token after it takes the character,
    // typing it here is valid and the separator fills itself in.
    if (tok == MaskToken::Sep) {
        MaskToken next = MaskToken::Any;
        uint32_t nextSep = 0;
        if (MaskTokenAt(p, pos + 1, &next, &nextSep) &&
            TokenIsMatch(next, nextSep, ch)) {
            return true;
        }
    }
    return false;
}

// Append one codepoint as UTF-8.
static void PushChar(StrBuilder& sb, uint32_t c) {
    if (c < 0x80) {
        sb.AppendChar((char)c);
    } else if (c < 0x800) {
        sb.AppendChar((char)(0xC0 | (c >> 6)));
        sb.AppendChar((char)(0x80 | (c & 0x3F)));
    } else if (c < 0x10000) {
        sb.AppendChar((char)(0xE0 | (c >> 12)));
        sb.AppendChar((char)(0x80 | ((c >> 6) & 0x3F)));
        sb.AppendChar((char)(0x80 | (c & 0x3F)));
    } else {
        sb.AppendChar((char)(0xF0 | (c >> 18)));
        sb.AppendChar((char)(0x80 | ((c >> 12) & 0x3F)));
        sb.AppendChar((char)(0x80 | ((c >> 6) & 0x3F)));
        sb.AppendChar((char)(0x80 | (c & 0x3F)));
    }
}

// The Number arm of mask(): regroup the integer part in threes, keep at most
// `fraction` decimals, and put the sign back on the front.
static Str MaskNumber(Arena* a, const MaskPattern& p, Str text) {
    if (!p.separator) {
        return StrDup(a, text);
    }
    // Remove the existing group separator, then split on the dot.
    StrBuilder bare;
    int dot = -1;
    for (int i = 0; i < text.len;) {
        uint32_t c = 0;
        int n = Utf8At(text, i, &c);
        if (c != p.separator) {
            if (c == '.' && dot < 0) {
                dot = bare.len;
            }
            for (int k = 0; k < n; k++) {
                bare.AppendChar(text.s[i + k]);
            }
        }
        i += n;
    }
    Str flat = Str(bare.els, bare.len);
    int intEnd = dot < 0 ? flat.len : dot;

    // Reverse the integer part for easier grouping, taking the sign out first
    // so the result cannot come out as `-,123`.
    uint32_t sign = 0;
    StrBuilder digits;
    for (int i = intEnd - 1; i >= 0; i--) {
        char c = flat.s[i];
        if (IsSign((uint32_t)(unsigned char)c) && !sign) {
            sign = (uint32_t)(unsigned char)c;
            continue;
        }
        digits.AppendChar(c);
    }
    StrBuilder grouped;
    for (int i = 0; i < digits.len; i++) {
        if (i > 0 && i % 3 == 0) {
            PushChar(grouped, p.separator);
        }
        grouped.AppendChar(digits.els[i]);
    }
    StrBuilder out;
    if (sign) {
        PushChar(out, sign);
    }
    for (int i = grouped.len - 1; i >= 0; i--) {
        out.AppendChar(grouped.els[i]);
    }
    if (dot >= 0 && p.fraction != 0) {
        out.AppendChar('.');
        int kept = 0;
        for (int i = intEnd + 1; i < flat.len;) {
            uint32_t c = 0;
            int n = Utf8At(flat, i, &c);
            if (p.fraction >= 0 && kept >= p.fraction) {
                break;
            }
            PushChar(out, c);
            kept++;
            i += n;
        }
    }
    return StrDup(a, Str(out.els, out.len));
}

Str MaskApply(Arena* a, const MaskPattern& p, Str text) {
    if (MaskIsNone(p)) {
        return StrDup(a, text);
    }
    if (p.kind == MaskKind::Number) {
        return MaskNumber(a, p, text);
    }
    StrBuilder out;
    int ti = 0;
    int tokens = RopeOffsetToCharIndex(p.pattern, p.pattern.len);
    for (int pos = 0; pos < tokens; pos++) {
        if (ti >= text.len) {
            break;
        }
        MaskToken tok = MaskToken::Any;
        uint32_t sep = 0;
        MaskTokenAt(p, pos, &tok, &sep);
        uint32_t ch = 0;
        int n = Utf8At(text, ti, &ch);
        // Break if the expected character does not match.
        if (tok != MaskToken::Sep && !MaskIsValidAt(p, ch, pos)) {
            break;
        }
        uint32_t masked = TokenMaskChar(tok, sep, ch);
        PushChar(out, masked);
        // A separator the text did not supply is filled in without consuming
        // anything, so the next token sees the same character.
        if (ch == masked) {
            ti += n;
        }
    }
    return StrDup(a, Str(out.els, out.len));
}

Str MaskUnapply(Arena* a, const MaskPattern& p, Str maskText) {
    if (p.kind == MaskKind::Number) {
        if (!p.separator) {
            return StrDup(a, maskText);
        }
        StrBuilder out;
        bool hasDot = false;
        for (int i = 0; i < maskText.len;) {
            uint32_t c = 0;
            int n = Utf8At(maskText, i, &c);
            if (c != p.separator) {
                PushChar(out, c);
                hasDot = hasDot || c == '.';
            }
            i += n;
        }
        int len = out.len;
        if (hasDot) {
            while (len > 0 && out.els[len - 1] == '0') {
                len--;
            }
        }
        return StrDup(a, Str(out.els, len));
    }
    if (p.kind == MaskKind::None) {
        return StrDup(a, maskText);
    }
    // Pattern: Rust walks the tokens against the *character* at the same
    // index, so a separator drops out and everything else is kept.
    StrBuilder out;
    int tokens = RopeOffsetToCharIndex(p.pattern, p.pattern.len);
    int ti = 0;
    for (int pos = 0; pos < tokens; pos++) {
        uint32_t ch = 0;
        int n = RopeCharAt(maskText, ti, &ch);
        if (n == 0) {
            break;
        }
        MaskToken tok = MaskToken::Any;
        uint32_t sep = 0;
        MaskTokenAt(p, pos, &tok, &sep);
        if (TokenUnmaskChar(tok)) {
            PushChar(out, ch);
        }
        ti += n;
    }
    return StrDup(a, Str(out.els, out.len));
}

Str MaskPlaceholder(Arena* a, const MaskPattern& p) {
    if (p.kind != MaskKind::Pattern) {
        return {};
    }
    StrBuilder out;
    int tokens = RopeOffsetToCharIndex(p.pattern, p.pattern.len);
    for (int pos = 0; pos < tokens; pos++) {
        MaskToken tok = MaskToken::Any;
        uint32_t sep = 0;
        MaskTokenAt(p, pos, &tok, &sep);
        // MaskToken::placeholder: a separator shows itself, everything else an
        // underscore.
        PushChar(out, tok == MaskToken::Sep ? sep : (uint32_t)'_');
    }
    return StrDup(a, Str(out.els, out.len));
}

// Every mapping is one character to one character with the same UTF-16 length,
// so IME marked-range offsets stay valid across it; the UTF-8 byte length may
// shrink from 3 to 1, which is why the caller must go on using the normalized
// string for its byte offsets.
static uint32_t NormalizeChar(uint32_t ch) {
    if (ch >= 0xFF10 && ch <= 0xFF19) { // full-width digits 0-9
        return ch - 0xFF10 + '0';
    }
    switch (ch) {
        case 0xFF0B: // ＋
            return '+';
        case 0xFF0D: // －
        case 0x2212: // −
            return '-';
        case 0xFF0E: // ．
        case 0x3002: // 。
            return '.';
        case 0xFF0C: // ，
            return ',';
        default:
            return ch;
    }
}

Str NormalizeNumberInput(Arena* a, Str text) {
    bool any = false;
    for (int i = 0; i < text.len && !any;) {
        uint32_t c = 0;
        i += Utf8At(text, i, &c);
        any = NormalizeChar(c) != c;
    }
    if (!any) {
        return StrDup(a, text); // Rust's Cow::Borrowed
    }
    StrBuilder out;
    for (int i = 0; i < text.len;) {
        uint32_t c = 0;
        i += Utf8At(text, i, &c);
        PushChar(out, NormalizeChar(c));
    }
    return StrDup(a, Str(out.els, out.len));
}

} // namespace gpui
