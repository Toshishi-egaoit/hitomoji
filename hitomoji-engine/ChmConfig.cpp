#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <locale>
#include <codecvt>
#include "ChmConfig.h"
#include "ChmRomajiConverter.h"
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

    ChmKeytableParser::ClearOverrideTable();

    while (std::getline(is, rawLine))
    {
        ++lineNo;
        std::wstring errorMsg;
        bool bRet = true;

        // ---- 共通前処理（ここに集約） ----
        std::wstring rawTrim = Trim(rawLine);

        // 空行
        if (rawTrim.empty())
            continue;

        // コメント行
        if (rawTrim[0] == L';' || rawTrim[0] == L'#')
            continue;

        // セクション処理
        if (_parseSection(rawTrim, currentSection, errorMsg))
            continue;

        if (!errorMsg.empty())
        {
            m_errors.push_back({ lineNo, errorMsg });
            continue;
        }

        // ---- セクション別ディスパッチ ----

        if (currentSection == L"key-table")
        {
            std::wstring k, v;
            bRet = ChmKeytableParser::ParseLine(rawLine, k, v, errorMsg);

            if (bRet)
            {
                ChmKeytableParser::RegisterOverrideTable(k, v);
            }
            else if (errorMsg == L"reserved line")
            {
                bRet = true; // 予約行は無視
            }
        }
        else
        {
            bRet = _parseKeyValue(rawTrim, currentSection, errorMsg);
        }

        if (!bRet)
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

	ifs.imbue(std::locale(
		ifs.getloc(),
		new std::codecvt_utf8<wchar_t>
	));

    return LoadFromStream(ifs);
}

BOOL ChmConfig::GetBool(const std::wstring& section, const std::wstring& key, const BOOL bDefault) const
{
    std::wstring sec = Canonize(section);
    std::wstring k   = Canonize(key);

    auto itSec = m_config.find(sec);
    if (itSec == m_config.end())
		return bDefault;

    auto itKey = itSec->second.find(k);
    if (itKey == itSec->second.end())
		return bDefault;

	if (std::holds_alternative<bool>(itKey->second))
		return std::get<bool>(itKey->second) ? TRUE : FALSE; // 値が bool 型ならその値を返す

	return FALSE; // 型が合わない場合はbDefaultではなくFALSEを返す
}

LONG ChmConfig::GetLong(const std::wstring& section, const std::wstring& key, const LONG lDefault) const
{
    std::wstring sec = Canonize(section);
    std::wstring k   = Canonize(key);

    auto itSec = m_config.find(sec);
    if (itSec == m_config.end())
		return lDefault; // セクションが見つからない場合は既定値 0

    auto itKey = itSec->second.find(k);
    if (itKey == itSec->second.end())
        return lDefault;

        if (std::holds_alternative<long>(itKey->second))
        return std::get<long>(itKey->second);

    return lDefault;
}

std::wstring ChmConfig::GetString(const std::wstring& section, const std::wstring& key) const
{
    std::wstring sec = Canonize(section);
    std::wstring k   = Canonize(key);

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

// --- public helpers ---
std::wstring ChmConfig::Trim(const std::wstring& s)
{
    // whitespace characters: space(0x20), tab(0x09), CR(0x0D), LF(0x0A)
    const wchar_t* ws = L" \t\r\n";
    size_t start = s.find_first_not_of(ws);
    if (start == std::wstring::npos) return L"";
    size_t end = s.find_last_not_of(ws);
    std::wstring out = s.substr(start, end - start + 1);
	return out;
}

//  大文字の小文字化　＋　'_'の'-'化
std::wstring ChmConfig::Canonize(const std::wstring& s)
{
    std::wstring result = s;
    std::transform(result.begin(), result.end(),
                   result.begin(),
                   [](wchar_t c)
                   {
                       if (c == L'_') return L'-';   // unify '_' to '-'
                       return (wchar_t)towlower(c);
                   });
    return result;
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

bool ChmConfig::_tryParseBool(const std::wstring& s, bool& outValue)
{
    // case-insensitive
    std::wstring v = Canonize(s);

    if (v == L"true" || v == L"yes" || v == L"on" || v == L"1")
    {
        outValue = true;
    }
    else if (v == L"false" || v == L"no" || v == L"off" || v == L"0")
    {
        outValue = false;
    }
	else 
	{
		return false;
	}
	return true;
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
    // allowed: [-a-z0-9]* (Canonize後を前提)
    if (name.empty()) return false;

    for (size_t i = 0; i < name.size(); ++i)
    {
        wchar_t c = name[i];
        if (!((c >= L'a' && c <= L'z') ||
              (c >= L'0' && c <= L'9') ||
              c == L'-'))
        {
            return false;
        }
    }
    return true;
}

BOOL ChmConfig::_parseSection(const std::wstring& rawTrim,
                              std::wstring& currentSection,
                              std::wstring& errorMsg)
{
  // おせっかいチェック
  if ( rawTrim.front() != L'[' && rawTrim.back() == L']')
  {
    errorMsg = L"missing '[' at section header";
    return FALSE;
  }
  if ( rawTrim.front() == L'[' && rawTrim.back() != L']')
  {
    errorMsg = L"missing ']' at end of section header";
    return FALSE;
  }

  if ( rawTrim.front() == L'[' && rawTrim.back() == L']')
  {
      std::wstring section = Trim(rawTrim.substr(1, rawTrim.size() - 2));
      section = Canonize(section);

      if (!_isValidName(section))
      {
          errorMsg = L"invalid section name";
          return FALSE;
      }

      currentSection = section;
      return TRUE;
  }

  // みしょりのケース
  return FALSE;
}

BOOL ChmConfig::_parseKeyValue(const std::wstring& rawTrim,
                               const std::wstring& currentSection,
                               std::wstring& errorMsg)
{
    size_t pos = rawTrim.find(L'=');
    if (pos == std::wstring::npos)
    {
        errorMsg = L"missing '=' in key-value pair";
        return FALSE;
    }

    if (currentSection.empty())
    {
        errorMsg = L"key-value pair outside of any section";
        return FALSE;
    }

    std::wstring key = Trim(rawTrim.substr(0, pos));
    key = Canonize(key);

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

    std::wstring v = Trim(rawTrim.substr(pos + 1));

	bool bValue = true;
    if (_tryParseBool(v, bValue))
    {
        m_config[currentSection][key] = bValue;
        return TRUE;
    }
    long n = 0;
    if (_tryParseLong(v, n))
    {
        m_config[currentSection][key] = n;
        return TRUE;
    }

    m_config[currentSection][key] = v;
    return TRUE;
}

