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
#include "ChmKeyEvent.h"
#include "utils.h"

#define MAX_ERROR_COUNT 100

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

void ChmConfig::InitConfig()
{
    m_config.clear();
    m_errors.clear();
	// function-key テーブルを初期化
	ChmKeyEvent::InitFunctionKey();
	// key-table テーブルを初期化
    ChmKeytableParser::ClearOverrideTable();

}

// --- include 対応内部ローダ ---
// NOTE: m_currentFile をエラー表示用に利用する設計
BOOL ChmConfig::_LoadStreamInternal(std::wistream& is,
                                     const std::wstring& fileName,
                                     BOOL isMain,
                                     std::wstring currentSection)
{
    // ファイル名はここで一元管理する
    std::wstring prevFile = m_currentFile;
    m_currentFile = fileName;

    std::wstring rawLine;
    size_t lineNo = 0;

    while (std::getline(is, rawLine))
    {
        ++lineNo;

        ParseResult errorMsg;
        bool bRet = true;

        std::wstring rawTrim = Trim(rawLine);

        if (rawTrim.empty())
            continue;

        if (rawTrim[0] == L';' || rawTrim[0] == L'#')
            continue;

        // --- @include 対応（main のみ） ---
        if (isMain && rawTrim.rfind(L"@include", 0) == 0)
        {
            std::wstring incFile = Trim(rawTrim.substr(8));

            if (incFile.empty())
            {
                m_errors.push_back({ m_currentFile, lineNo, L"@include without file name" });
                continue;
            }

            std::wstring baseDir;
            PWSTR path = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path)))
            {
                baseDir = path;
                CoTaskMemFree(path);
                baseDir += L"\\hitomoji\\";
            }

            std::wstring fullPath = baseDir + incFile;

            std::wifstream incIfs(fullPath);
            if (!incIfs.is_open())
            {
                m_errors.push_back({ m_currentFile, lineNo, L"cannot open include file: " + incFile });
                continue;
            }

            incIfs.imbue(std::locale(
                incIfs.getloc(),
                new std::codecvt_utf8<wchar_t>
            ));

            // 再帰呼び出し時に fileName を渡す
            _LoadStreamInternal(incIfs, fullPath, FALSE, currentSection);
            continue;
        }

        // ---- セクション処理 ----
        if (currentSection != L"key-table") {
            if (rawTrim.front() != L'[' && rawTrim.back() == L']')
            {
                m_errors.push_back({ m_currentFile, lineNo, L"missing '[' at section header" });
                continue;
            }
            if (rawTrim.front() == L'[' && rawTrim.back() != L']')
            {
                m_errors.push_back({ m_currentFile, lineNo, L"missing ']' at end of section header" });
                continue;
            }
        }

        if (_parseSection(rawTrim, currentSection, errorMsg))
        {
            continue;
        }
        if (errorMsg.level != ParseLevel::None) goto WriteLog;

		{
			std::wstring key;
			std::wstring value;
			bRet = _divideRawTrim(rawTrim, key, value, errorMsg);

            // セクション未指定チェック
            if (errorMsg.level == ParseLevel::None && currentSection.empty())
            {
                SetError(errorMsg, L"key-value pair found before any section header");
            }

			if (errorMsg.level != ParseLevel::None)
			{
				_addErrorOrInfo(errorMsg, lineNo);
				continue;
			}

			if (currentSection == L"key-table")
			{
				bRet = ChmKeytableParser::ParseLine(rawLine, key, value, errorMsg);
			}
			else if (currentSection == L"function-key")
			{
				bRet = ChmKeyEvent::ParseFunctionKey(key, value, errorMsg);
			}
			else
			{
				bRet = _parseValue(key, value, currentSection, errorMsg);
			}
		}

WriteLog:
		// Error／Infoの記録
		if (_addErrorOrInfo(errorMsg, lineNo) == false) {
			// エラー／情報の上限超過
			return FALSE;
		}
    }

    m_currentFile = prevFile; // 呼び出し元へ復帰
    return !HasErrors();
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

    InitConfig();

    BOOL bRet = _LoadStreamInternal(ifs, path, /*isMain*/ TRUE, L"");

	OutputDebugStringWithInt(L"   > LoadFile end. m_errors.size(): %d\n", m_errors.size());
	OutputDebugStringWithInt(L"   >               m_infos.size(): %d\n", m_infos.size());

	return bRet;
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

std::wstring ChmConfig::DumpInfos() const
{
	return _DumpInfos();
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
        out += e.fileName;
        out += L"(";
        out += std::to_wstring(e.lineNo);
        out += L"): ";
        out += e.message;
        out += L"\n";
    }
    return out;
}

std::wstring ChmConfig::_DumpInfos() const
{
    std::wstring out;
    for (const auto& e : m_infos)
    {
        out += e.fileName;
        out += L"(";
        out += std::to_wstring(e.lineNo);
        out += L"): ";
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
                              ParseResult& errorMsg)
{

	if ( rawTrim.front() == L'[' && rawTrim.back() == L']')
	  {
		  std::wstring section = Trim(rawTrim.substr(1, rawTrim.size() - 2));
		  section = Canonize(section);

	if (!_isValidName(section))
	{
		SetError(errorMsg, L"invalid section name") ;
		return FALSE;
	}

	if (section.empty())
	{
		SetError(errorMsg, L"empty section name") ;
		return FALSE;
	}

      currentSection = section;
      return TRUE;
  }

  // みしょりのケース
  return FALSE;
}

BOOL ChmConfig::_divideRawTrim(const std::wstring& rawTrim,
                               std::wstring& key,
                               std::wstring& value,
                              ParseResult& errorMsg)
{
    size_t pos = rawTrim.find(L'=');
    if (pos == std::wstring::npos)
    {
		SetError(errorMsg, L"missing '=' in key-value pair");
        return FALSE;
    }

	// ここではぶんかつだけをおこなう。canonizeはしない
    key = Trim(rawTrim.substr(0, pos));

    if (key.empty())
    {
		SetError(errorMsg, L"empty key name");
        return FALSE;
    }

    value = Trim(rawTrim.substr(pos + 1));
	return TRUE;
}

BOOL ChmConfig::_parseValue(const std::wstring& keyTrim,
                               const std::wstring& valueTrim,
                               const std::wstring& currentSection,
                               ParseResult& errorMsg)
{
    std::wstring key = Canonize(keyTrim);

	// --- key の妥当性チェック
    if (!_isValidName(key))
    {
        SetError(errorMsg, L"invalid key name:" + keyTrim);
        return FALSE;
    }

	if (_isDuplicateKey(currentSection,key))
	{
		SetInfo(errorMsg, 
			L"duplicate key overwritten: " + currentSection + L"." + key);
		// 重複はエラーではないのでReturnはしない
	}

	// --- value の型判定（bool → long → string の順で試す）
	bool bValue = true;
    if (_tryParseBool(valueTrim, bValue))
    {
        m_config[currentSection][key] = bValue;
        return TRUE;
    }

    long n = 0;
    if (_tryParseLong(valueTrim, n))
    {
        m_config[currentSection][key] = n;
        return TRUE;
    }

    m_config[currentSection][key] = valueTrim;
    return TRUE;
}

bool ChmConfig::_isDuplicateKey(const std::wstring& section, const std::wstring & canonizedKey) 
{
	auto sectionMap = m_config.find(section) ;

	if ( sectionMap == m_config.end()) return false;

	return (sectionMap->second.find(canonizedKey) != sectionMap->second.end());
}

bool ChmConfig::_addErrorOrInfo(ParseResult& errorMsg, size_t lineNo)
{
	if (m_errors.size() + m_infos.size() >= MAX_ERROR_COUNT) {
		// エラー／情報が多すぎる場合はこれ以上記録しない（無限ループ対策）
		m_errors.push_back({ m_currentFile, lineNo, L"Too many errors or infos." });
		return false;
	}
	if (errorMsg.level == ParseLevel::Error)
		m_errors.push_back({ m_currentFile, lineNo, errorMsg.message });
	else if (errorMsg.level == ParseLevel::Info)
		m_infos.push_back({ m_currentFile, lineNo, errorMsg.message });
	return true;
}