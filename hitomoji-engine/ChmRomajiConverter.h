// ChmRomajiConverter.h
#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "ChmConfig.h"


extern const std::unordered_map<std::wstring, std::wstring> _baseTable ;
extern std::unordered_map<std::wstring, std::wstring> _overrideTable;

// rawInput (ASCII) を走査して、ひらがなへ変換するクラス
class ChmRomajiConverter {
public:
    struct Unit {
        std::wstring output;   // 確定した1ユニットの表示文字
        size_t rawLength = 0;  // 消費した raw 文字数
    };

    static void convert(const std::wstring& rawInput,
                        std::wstring& converted,
                        std::wstring& pending,
                        bool isBackspaceSymbol);

    static bool TryConvertOne(const std::wstring& rawInput,
                              size_t pos,
                              Unit& out);

    static size_t GetLastRawUnitLength();

    static std::wstring HiraganaToKatakana(const std::wstring& hira);

private:
    static size_t _lastRawUnitLength;

    static const std::unordered_set<wchar_t>& sokuonConsonants();
};

class ChmKeytableParser {
public:

	static bool ParseLine(const std::wstring& line,
                                  std::wstring& left,
                                  std::wstring& right,
                                  std::wstring& error);

	static void RegisterOverrideTable(const std::wstring& key,
											  const std::wstring& value)
	{
		_overrideTable[key] = value;
	}

	static void ClearOverrideTable()
	{
		_overrideTable.clear();
	}

private:
    static std::wstring Trim(const std::wstring& s);
};

