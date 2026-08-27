/* The slice equality contract in base.h. The length rejection is inline so
   callers avoid entering the case-insensitive comparison for the common
   mismatch, including two slices that happen to share their first byte. */

#include "Test.h"

static void CaseInsensitiveEqualityRejectsLengthFirst() {
    char text[] = "Alpha";
    Str whole(text, 5);
    Str prefix(text, 4);

    utassert(base::StrEqI(whole, "aLPHA"));
    utassert(!base::StrEqI(prefix, whole));
    utassert(!base::StrEqI(whole, "alpha!"));
}

static void CaseInsensitiveEqualityKeepsEmptySliceSemantics() {
    char empty[] = "";
    utassert(base::StrEqI(Str{}, Str{}));
    utassert(base::StrEqI(Str(empty), ""));
    utassert(base::StrEqI(Str{}, ""));
    utassert(!base::StrEqI(Str{}, "x"));
}

static void CaseInsensitivePrefixUsesBothOverloads() {
    Str text = StrL("Alpha");
    utassert(base::StrStartsWithI(text, "aL"));
    utassert(base::StrStartsWithI(text, StrL("ALP")));
    utassert(!base::StrStartsWithI(text, "Alphas"));
    utassert(base::StrStartsWithI(Str{}, ""));
    utassert(!base::StrStartsWithI(Str{}, "a"));
}

void TestStr() {
    TestSuite("str");
    CaseInsensitiveEqualityRejectsLengthFirst();
    CaseInsensitiveEqualityKeepsEmptySliceSemantics();
    CaseInsensitivePrefixUsesBothOverloads();
}
