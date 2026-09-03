#ifndef GPUI_SHELL_FETCH_H_
#define GPUI_SHELL_FETCH_H_

#include "shell/capability.h"
#include "sys/http.h"

namespace gpui::shell {

constexpr int kFetchMaxBody = 8 * 1024 * 1024;
constexpr int kFetchMaxRedirects = 10;

struct FetchResult {
    int status = 0;
    Str url;
    Str body;
    Str error;

    void Free();
};

// `parse_method`: a token check, not a list. Which method may reach which
// host on which path is `Capabilities::MayRequest`'s decision and it already
// takes the method; what is refused here is a string that is not an HTTP
// method at all — an empty field, a space, a quote.
//
// The standing repository boundary is one system-backed GET, so a method that
// is a method but is not GET is still refused, by `FetchAuthorizeMethod`
// rather than by this.
bool FetchIsHttpMethod(Str method);

bool FetchAuthorizeGet(Str url, const Capabilities& capabilities,
                       Str* error = nullptr);
bool FetchGet(Str url, const Capabilities& capabilities, FetchResult* out);

using FetchHttpGet = bool (*)(Str url, HttpRsp* out);
void FetchSetHttpGetForTests(FetchHttpGet get);

} // namespace gpui::shell
#endif // GPUI_SHELL_FETCH_H_
