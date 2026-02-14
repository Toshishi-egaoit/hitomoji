#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <assert.h>
#include "ChmConfig.h"
#include "utils.h"

static std::wstring GetConfigPath()
{
    PWSTR path = nullptr;
    std::wstring result;

    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path)))
    {
        result = path;
        CoTaskMemFree(path);
        result += L"\\hitomoji\\hitomoji.ini";
    }
    return result;
}

ChmConfig::ChmConfig()
{
    InitConfig();
}

BOOL ChmConfig::LoadFromStream(std::wistream& is)
{
    InitConfig();

    std::wstring rawLine;
    std::wstring currentSection;
    size_t lineNo = 0;

    while (std::getline(is, rawLine))
    {
        ++lineNo;
        std::wstring errorMsg;
        if (!_parseLine(rawLine, currentSection, errorMsg))
        {
            m_errors.push_back({ lineNo, errorMsg });
        }
    }

    return TRUE;
}

void ChmConfig::InitConfig()
{
    m_config.clear();
    m_errors.clear();
}

BOOL ChmConfig::LoadFile(const std::wstring& fileName)
{
    OutputDebugStringWithString(L"[Hitomoji] ChmConfig::LoadFile(%s) called", fileName.c_str());
    std::wstring path = fileName.empty() ? GetConfigPath() : fileName;

    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        OutputDebugStringWithString(L"   > File not found(%s)", path.c_str());
        return FALSE;
    }

    std::wifstream ifs(path);
    if (!ifs.is_open())
    {
        OutputDebugStringWithString(L"   > cannot open(%s)", path.c_str());
        return FALSE;
    }

    return LoadFromStream(ifs);
}

BOOL ChmConfig::GetBool(const std::wstring& section, const std::wstring& key) const
{
    std::wstring sec = _normalize(section);
    std::wstring k   = _normalize(key);

    auto itSec = m_config.find(sec);
    if (itSec == m_config.end())
        return FALSE;

    auto itKey = itSec->second.find(k);
    if (itKey == itSec->second.end())
        return FALSE;

        if (std::holds_alternative<bool>(itKey->second))
        return std::get<bool>(itKey->second) ? TRUE : FALSE;

    return FALSE;
}

LONG ChmConfig::GetLong(const std::wstring& section, const std::wstring& key) const
{
    std::wstring sec = _normalize(section);
    std::wstring k   = _normalize(key);

    auto itSec = m_config.find(sec);
    if (itSec == m_config.end())
        return 0;

    auto itKey = itSec->second.find(k);
    if (itKey == itSec->second.end())
        return 0;

        if (std::holds_alternative<long>(itKey->second))
        return std::get<long>(itKey->second);

    return 0;
}

std::wstring ChmConfig::GetString(const std::wstring& section, const std::wstring& key) const
{
    std::wstring sec = _normalize(section);
    std::wstring k   = _normalize(key);

    auto itSec = m_config.find(sec);
    if (itSec == m_config.end())
        return L"";

    auto itKey = itSec->second.find(k);
    if (itKey == itSec->second.end())
        return L"";

    if (std::holds_alternative<std::wstring>(itKey->second))
        return std::get<std::wstring>(itKey->second);

    return L"";
}

std::wstring ChmConfig::Dump() const
{
	return _Dump();
}

std::wstring ChmConfig::DumpErrors() const
{
	return _DumpErrors();
}

// --- private helpers ---

std::wstring ChmConfig::_Dump() const
{
    std::wstring out;
    for (const auto& sec : m_config)
    {
        out += L"[" + sec.first + L"]\n";
        for (const auto& kv : sec.second)
        {
            out += L"  " + kv.first + L" = ";
                        if (std::holds_alternative<bool>(kv.second))
            {
                out += std::get<bool>(kv.second) ? L"true" : L"false";
            }
            else if (std::holds_alternative<long>(kv.second))
            {
                out += std::to_wstring(std::get<long>(kv.second));
            }
            else
            {
                out += std::get<std::wstring>(kv.second);
            }
            out += L"\n";
        }
    }

    return out;
}

std::wstring ChmConfig::_DumpErrors() const
{
    std::wstring out;
    for (const auto& e : m_errors)
    {
        out += std::to_wstring(e.lineNo);
        out += L": ";
        out += e.message;
        out += L"\n";
    }

    return out;
}



bool ChmConfig::_tryParseBool(const std::wstring& s, long& outValue)
{
    // case-insensitive
    std::wstring v;
    v.reserve(s.size());
    for (wchar_t c : s)
        v.push_back((c >= L'A' && c <= L'Z') ? (c - L'A' + L'a') : c);

    if (v == L"true" || v == L"yes" || v == L"on" || v == L"1")
    {
        outValue = 1;
        return true;
    }
    if (v == L"false" || v == L"no" || v == L"off" || v == L"0")
    {
        outValue = 0;
        return true;
    }
    return false;
}

bool ChmConfig::_tryParseLong(const std::wstring& s, long& outValue)
{
    if (s.empty()) return false;

    wchar_t* endp = nullptr;
    const wchar_t* begin = s.c_str();
    long v = wcstol(begin, &endp, 10);

    // 全体が数値として消費されていることを確認
    if (endp && *endp == L'\0')
    {
        outValue = v;
        return true;
    }
    return false;
}



bool ChmConfig::_isValidName(const std::wstring& name)
{
    // allowed: [_a-zA-Z][_a-zA-Z0-9.-]*
    if (name.empty()) return false;

    wchar_t c0 = name[0];
    if (!( (c0 >= L'a' && c0 <= L'z') ||
           (c0 >= L'A' && c0 <= L'Z') ||
           c0 == L'_' ))
    {
        return false;
    }

    for (size_t i = 1; i < name.size(); ++i)
    {
        wchar_t c = name[i];
        if (!( (c >= L'a' && c <= L'z') ||
               (c >= L'A' && c <= L'Z') ||
               (c >= L'0' && c <= L'9') ||
               c == L'_' || c == L'-' || c == L'.' ))
        {
            return false;
        }
    }
    return true;
}



static std::wstring _trim(const std::wstring& s)
{
    // whitespace characters: space(0x20), tab(0x09), CR(0x0D), LF(0x0A)
    const wchar_t* ws = L" \t\r\n";
    size_t start = s.find_first_not_of(ws);
    if (start == std::wstring::npos) return L"";
    size_t end = s.find_last_not_of(ws);
    std::wstring out = s.substr(start, end - start + 1);
	return out;
}

std::wstring ChmConfig::_normalize(const std::wstring& s)
{
    // 小文字化のみ（trimは行わない）
    std::wstring out = s;
    for (auto& c : out)
    {
        if (c >= L'A' && c <= L'Z') c = c - L'A' + L'a';
    }
    return out;
}

BOOL ChmConfig::_parseLine(const std::wstring& rawLine, std::wstring& currentSection, std::wstring& errorMsg)
{
    OutputDebugStringWithString(L"_parseLine: %s", rawLine.c_str());

    // まず trim
    std::wstring rawTrim = _trim(rawLine);
    if (rawTrim.empty()) return TRUE;

    // 解析用に小文字化（trimはしない）
    std::wstring normalizedLine = _normalize(rawTrim);
    if (normalizedLine.empty()) return TRUE;

    // コメント行
    if (normalizedLine[0] == L';' || normalizedLine[0] == L'#') return TRUE;

        // セクション行 [Section]
    if (normalizedLine.front() == L'[' && normalizedLine.back() == L']')
    {
        // [] 内を取り出して trim（前後空白許容）
        std::wstring section = _trim(normalizedLine.substr(1, normalizedLine.size() - 2));
        if (!_isValidName(section))
        {
            errorMsg = L"invalid section name";
            return FALSE;
        }
        currentSection = section;
        return TRUE;
    }

    // key=value パース
    size_t pos = normalizedLine.find(L'=');
    if (pos == std::wstring::npos)
    {
        // おせっかいチェック：セクション書きかけかも
        if (normalizedLine.front() == L'[' && normalizedLine.back() != L']')
        {
            errorMsg = L"missing ']' at end of section header";
            return FALSE;
        }
        if (normalizedLine.front() != L'[' && normalizedLine.back() == L']')
        {
            errorMsg = L"missing '[' at beginning of section header";
            return FALSE;
        }

        // '=' がない行は文法エラー
        errorMsg = L"missing '=' in key-value pair";
        return FALSE;
    }

        std::wstring key = _trim(normalizedLine.substr(0, pos));
    std::wstring v = _trim(normalizedLine.substr(pos + 1)); // 型判定用

    if (key.empty())
    {
        errorMsg = L"empty key name";
        return FALSE;
    }
    if (!_isValidName(key))
    {
        errorMsg = L"invalid key name";
        return FALSE;
    }

    // セクション未定義の場合はエラー
    if (currentSection.empty())
    {
        errorMsg = L"key-value pair outside of any section";
        return FALSE;
    }

    // value 型判定（内部は long / string に正規化）
    long n = 0;
        if (_tryParseBool(v, n))
    {
        m_config[currentSection][key] = (n != 0); // boolとして保存
        return TRUE;
    }
    if (_tryParseLong(v, n))
    {
        m_config[currentSection][key] = n;
        return TRUE;
    }

    // それ以外は string（rawTrim から再抽出して trim）
	// TODO:現状ではTRUEを文字列として扱うことができない（=true は bool として解釈される）。クオート処理が必要。
	// TODO:同様にStringの先頭に空白記号を入れることもできない。クオート処理が必要。
    size_t rawPos = rawTrim.find(L'=');
    if (rawPos != std::wstring::npos)
    {
        std::wstring rawValue = _trim(rawTrim.substr(rawPos + 1));
        m_config[currentSection][key] = rawValue;
    }
    else
    {
        m_config[currentSection][key] = L"";
    }
    return TRUE;
}
