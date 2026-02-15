// ChmRomajiConverter.h
#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "ChmConfig.h"

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
                        bool isDisplayKana,
                        bool isBackspaceSymbol);

    static bool TryConvertOne(const std::wstring& rawInput,
                              size_t pos,
                              Unit& out,
                              bool isSymbolMode);

    static size_t GetLastRawUnitLength();

    static std::wstring HiraganaToKatakana(const std::wstring& hira);

private:
    static size_t _lastRawUnitLength;

    static const std::unordered_map<std::wstring, std::wstring>& table();
    static const std::unordered_set<wchar_t>& sokuonConsonants();
};
