/* Ported from crates/base/src/otp_input.rs edit_value.
 *
 * Rust's own cases there drive a window; edit_value is the pure half and is
 * where the rules are — digits only, backspace pops, full-width digits fold
 * onto plain ones, and a full code refuses more. */

#include "Test.h"

static void OnlyDigitsAreTaken() {
    OtpState s;
    utassert(OtpEditValue(&s, 0, '1'));
    utassert(OtpEditValue(&s, 0, '2'));
    utassert(base::StrEqI(Str(s.value), "12"));
    // A letter, a space and a symbol all leave the value where it was.
    utassert(!OtpEditValue(&s, 0, 'a'));
    utassert(!OtpEditValue(&s, 0, ' '));
    utassert(!OtpEditValue(&s, 0, '-'));
    utassert(base::StrEqI(Str(s.value), "12"));
}

static void FullWidthDigitsFoldOntoPlainOnes() {
    OtpState s;
    // U+FF13 and U+FF17, which an IME produces.
    utassert(OtpEditValue(&s, 0, 0xFF13));
    utassert(OtpEditValue(&s, 0, 0xFF17));
    utassert(base::StrEqI(Str(s.value), "37"));
    utassert(OtpDigitChar(0xFF10) == '0');
    utassert(OtpDigitChar(0xFF19) == '9');
    utassert(OtpDigitChar(0xFF1A) == 0);
}

static void BackspacePopsTheLastDigit() {
    OtpState s;
    OtpEditValue(&s, 0, '4');
    OtpEditValue(&s, 0, '5');
    utassert(OtpEditValue(&s, KeyBack, 0));
    utassert(base::StrEqI(Str(s.value), "4"));
    utassert(OtpEditValue(&s, KeyBack, 0));
    utassert(base::StrEqI(Str(s.value), ""));
    // Nothing left to pop.
    utassert(!OtpEditValue(&s, KeyBack, 0));
}

static void AFullCodeRefusesMore() {
    OtpState s;
    s.length = 4;
    utassert(OtpEditValue(&s, 0, '1'));
    utassert(OtpEditValue(&s, 0, '2'));
    utassert(OtpEditValue(&s, 0, '3'));
    utassert(OtpEditValue(&s, 0, '4'));
    utassert(base::StrEqI(Str(s.value), "1234"));
    // The run is not shifted along; the digit is simply dropped.
    utassert(!OtpEditValue(&s, 0, '5'));
    utassert(base::StrEqI(Str(s.value), "1234"));
    utassert(s.len == 4);
}

void TestOtpInput() {
    TestSuite("otp_input");
    OnlyDigitsAreTaken();
    FullWidthDigitsFoldOntoPlainOnes();
    BackspacePopsTheLastDigit();
    AFullCodeRefusesMore();
}
