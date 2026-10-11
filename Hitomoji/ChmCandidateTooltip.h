#pragma once
#include "ChmCandidatePage.h"
#include <sstream>
#include <iomanip>

namespace ChmCandidateTooltip {
inline std::wstring Glyph(uint32_t cp)
{
    if (!cp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return {};
    if (cp <= 0xFFFF) return std::wstring(1, static_cast<wchar_t>(cp));
    cp -= 0x10000;
    return {static_cast<wchar_t>(0xD800 + (cp >> 10)), static_cast<wchar_t>(0xDC00 + (cp & 0x3FF))};
}
inline std::wstring Title(const ChmKanjiInfo& info)
{
    auto glyph = Glyph(info.codePoint);
    if (glyph.empty()) return {};
    std::wostringstream text;
    text << glyph << L"  U+" << std::uppercase << std::hex << std::setfill(L'0')
         << std::setw(4) << info.codePoint;
    return text.str();
}
inline std::wstring Body(const ChmKanjiInfo& info)
{
    std::wstring text;
    auto line = [&](const std::wstring& value) {
        if (value.empty()) return;
        if (!text.empty()) text += L"\n";
        text += value;
    };
    switch (info.category) {
    case ChmKanjiCategory::Education1: case ChmKanjiCategory::Education2:
    case ChmKanjiCategory::Education3: case ChmKanjiCategory::Education4:
    case ChmKanjiCategory::Education5: case ChmKanjiCategory::Education6:
        line(L"教育漢字（小学" + std::to_wstring(static_cast<int>(info.category) + 1) + L"年）"); break;
    case ChmKanjiCategory::Joyo: line(L"常用漢字"); break;
    case ChmKanjiCategory::Jinmeiyo: line(L"人名用漢字"); break;
    case ChmKanjiCategory::Other: line(L"表外漢字"); break;
    default: break;
    }
    std::wstring jis;
    if (info.jis.level >= 1 && info.jis.level <= 4)
        jis = L"JIS第" + std::to_wstring(info.jis.level) + L"水準";
    if (info.jis.plane && info.jis.ku && info.jis.ten) {
        if (!jis.empty()) jis += L"  ";
        jis += std::to_wstring(info.jis.plane) + L"面" + std::to_wstring(info.jis.ku)
            + L"区" + std::to_wstring(info.jis.ten) + L"点";
    }
    line(jis);
    if (info.radical.number) {
        auto glyph = Glyph(info.radical.codePoint);
        std::wstring radical = L"部首：" + glyph;
        if (!info.radical.name.empty()) radical += L"（" + info.radical.name + L"）";
        if (glyph.empty() && info.radical.name.empty()) radical += std::to_wstring(info.radical.number);
        line(radical);
    }
    if (info.strokeCount) line(std::to_wstring(info.strokeCount) + L"画");
    return text;
}
inline std::wstring Variants(const ChmKanjiInfo& info)
{
    std::wstring variants;
    for (const auto& variant : info.variants) {
        auto glyph = Glyph(variant.codePoint);
        if (glyph.empty()) continue;
        if (!variants.empty()) variants += L" ";
        variants += glyph;
    }
    return variants;
}
}

