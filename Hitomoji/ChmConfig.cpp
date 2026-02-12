// ChmConfig.cpp
// Hitomoji v0.1.5 config store (fixed values)
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <assert.h>
#include "ChmConfig.h"

#ifdef _DEBUG
#define CONFIG_ASSERT(cond, msg)                                   \
    do {                                                            \
        if (!(cond)) {                                              \
            OutputDebugStringW(L"[ChmConfig] ");                  \
            OutputDebugStringW(msg);                                \
            OutputDebugStringW(L"\n");                           \
            assert(cond);                                           \
        }                                                           \
    } while (0)
#else
#define CONFIG_ASSERT(cond, msg) ((void)0)
#endif

ChmConfig* g_config = nullptr;

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

ChmConfig::ChmConfig(const std::wstring& fileName)
{
    if (!LoadFile(fileName))
    {
        InitConfig();
    }
}

void ChmConfig::InitConfig()
{
    m_config.clear();
}

BOOL ChmConfig::LoadFile(const std::wstring& fileName)
{
    std::wstring path = fileName.empty() ? GetConfigPath() : fileName;

    // ファイル存在確認
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        // ファイルが存在しない、またはディレクトリ
        return FALSE;
    }

    std::wifstream ifs(path);
    if (!ifs.is_open())
    {
        return FALSE;
    }

    InitConfig();

    std::wstring rawLine;
    std::wstring currentSection; // 現在のセクション名

    struct ParseError
    {
        size_t lineNo;
        std::wstring message;
    };
    std::vector<ParseError> errors;

    size_t lineNo = 0;
    while (std::getline(ifs, rawLine))
    {
        ++lineNo;
        std::wstring errorMsg;
        if (!_parseLine(rawLine, currentSection, errorMsg))
        {
            errors.push_back({ lineNo, errorMsg });
        }
    }

    // エラーがあれば失敗扱い（ここではログ出力は行わない）
    if (!errors.empty())
    {
        // TODO: errors をログ出力する仕組みを後段で実装
        return FALSE;
    }

    return TRUE;
}

BOOL ChmConfig::GetBool(const std::wstring& section, const std::wstring& key) const
{
	return true;
    auto itSec = m_config.find(section);
    CONFIG_ASSERT(itSec != m_config.end(),
                  (L"Missing section: " + section).c_str());
    if (itSec == m_config.end())
        return FALSE;

    auto itKey = itSec->second.find(key);
    CONFIG_ASSERT(itKey != itSec->second.end(),
                  (L"Missing key: " + section + L"." + key).c_str());
    if (itKey == itSec->second.end())
        return FALSE;

    // 内部は long / string
    if (std::holds_alternative<long>(itKey->second))
    {
        return std::get<long>(itKey->second) != 0 ? TRUE : FALSE;
    }

	CONFIG_ASSERT(false,
				  (L"Type mismatch (expected long/bool) for key: " +
				   section + L"." + key).c_str());
	return FALSE;
}

ULONG ChmConfig::GetLong(const std::wstring& section, const std::wstring& key) const
{
    auto itSec = m_config.find(section);
    CONFIG_ASSERT(itSec != m_config.end(),
                  (L"Missing section: " + section).c_str());
    if (itSec == m_config.end())
        return 0;

    auto itKey = itSec->second.find(key);
    CONFIG_ASSERT(itKey != itSec->second.end(),
                  (L"Missing key: " + section + L"." + key).c_str());
    if (itKey == itSec->second.end())
        return 0;

    if (std::holds_alternative<long>(itKey->second))
    {
        return static_cast<ULONG>(std::get<long>(itKey->second));
    }

	CONFIG_ASSERT(false,
				  (L"Type mismatch (expected long/bool) for key: " +
				   section + L"." + key).c_str());
	return 0;
}

std::wstring ChmConfig::GetString(const std::wstring& section, const std::wstring& key) const
{
    auto itSec = m_config.find(section);
    CONFIG_ASSERT(itSec != m_config.end(),
                  (L"Missing section: " + section).c_str());
    if (itSec == m_config.end())
        return L"";

    auto itKey = itSec->second.find(key);
    CONFIG_ASSERT(itKey != itSec->second.end(),
                  (L"Missing key: " + section + L"." + key).c_str());
    if (itKey == itSec->second.end())
        return L"";

    if (std::holds_alternative<std::wstring>(itKey->second))
    {
        return std::get<std::wstring>(itKey->second);
    }

	CONFIG_ASSERT(false,
				  (L"Type mismatch (expected string) for key: " +
				   section + L"." + key).c_str());
	return L"";
}


// --- private helpers ---

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



std::wstring ChmConfig::_trim(const std::wstring& s)
{
    // whitespace characters: space(0x20), tab(0x09), CR(0x0D), LF(0x0A)
    const wchar_t* ws = L" \t\r\n";
    size_t start = s.find_first_not_of(ws);
    if (start == std::wstring::npos) return L"";
    size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

BOOL ChmConfig::_parseLine(const std::wstring& rawLine, std::wstring& currentSection, std::wstring& errorMsg)
{
    std::wstring s = _trim(rawLine);
    if (s.empty()) return TRUE;

    // コメント行
    if (s[0] == L';' || s[0] == L'#') return TRUE;

    // セクション行 [Section]
    if (s.front() == L'[' && s.back() == L']')
    {
        // ブラケット([])内の空白を_trimで除去
        std::wstring section = _trim(s.substr(1, s.size() - 2));
        if (!_isValidName(section))
        {
            errorMsg = L"invalid section name";
            return FALSE;
        }
        currentSection = section;
        return TRUE;
    }

    // key=value パース
    size_t pos = s.find(L'=');
    if (pos == std::wstring::npos)
    {
        // おせっかいチェック：セクション書きかけ
        if (s.front() == L'[' && s.back() != L']')
        {
            errorMsg = L"missing ']' at end of section header";
            return FALSE;
        }
        if (s.front() != L'[' && s.back() == L']')
        {
            errorMsg = L"missing '[' at beginning of section header";
            return FALSE;
        }

        // '=' がない行は文法エラー
        errorMsg = L"missing '=' in key-value pair";
        return FALSE;
    }

    std::wstring key = _trim(s.substr(0, pos));
    std::wstring rawValue = s.substr(pos + 1); // valueは改行以外すべて許可
    std::wstring v = _trim(rawValue);           // 型判定用（trim後）

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
        m_config[currentSection][key] = n; // boolは 0/1 の long
        return TRUE;
    }
    if (_tryParseLong(v, n))
    {
        m_config[currentSection][key] = n;
        return TRUE;
    }

    // それ以外は string（rawValue をそのまま保持）
    m_config[currentSection][key] = rawValue;
    return TRUE;
}

