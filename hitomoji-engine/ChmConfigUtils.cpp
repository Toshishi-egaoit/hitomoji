#include <algorithm>
#include <cwctype>
#include "ChmConfigUtils.h"

namespace ChmStringUtil {

std::wstring Trim(const std::wstring& s)
{
    // whitespace characters: space(0x20), tab(0x09), CR(0x0D), LF(0x0A), BOM(0xFEFF)
    const wchar_t* ws = L" \t\r\n\xFEFF";
    size_t start = s.find_first_not_of(ws);
    if (start == std::wstring::npos) return L"";
    size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

std::wstring Canonize(const std::wstring& s)
{
    std::wstring result = s;
    std::transform(result.begin(), result.end(), result.begin(),
        [](wchar_t c)
        {
            if (c == L'_') return L'-';
            return static_cast<wchar_t>(towlower(c));
        });
    return result;
}

} // namespace ChmStringUtil

namespace ChmConfigParse {

void SetError(Result& r, const std::wstring& msg)
{
    r.level = Level::Error;
    r.message = msg;
}

void SetInfo(Result& r, const std::wstring& msg)
{
    r.level = Level::Info;
    r.message = msg;
}

} // namespace ChmConfigParse