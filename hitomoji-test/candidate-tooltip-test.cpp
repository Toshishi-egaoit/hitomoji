#include "gtest/gtest.h"
#include "../Hitomoji/ChmCandidateTooltip.h"

TEST(CandidateTooltipTest, OmitsMissingInformation)
{
    ChmKanjiInfo info;
    info.codePoint = 0x7A81;
    EXPECT_EQ(L"突  U+7A81", ChmCandidateTooltip::Title(info));
    EXPECT_TRUE(ChmCandidateTooltip::Body(info).empty());
    info.jis.ku = 38; // A partial coordinate must not be presented as valid.
    EXPECT_TRUE(ChmCandidateTooltip::Body(info).empty());
    info.codePoint = 0;
    EXPECT_TRUE(ChmCandidateTooltip::Title(info).empty());
}

TEST(CandidateTooltipTest, FormatsDetailsAndMultipleVariantsInOrder)
{
    ChmKanjiInfo info;
    info.codePoint = 0x5F01;
    info.category = ChmKanjiCategory::Education1;
    info.jis = {1, 42, 59, 1};
    info.radical = {55, 0x5EFE, L"にじゅうあし"};
    info.strokeCount = 5;
    info.variants = {{0x8FA8}, {0x8FAF}, {0x74E3}};
    EXPECT_EQ(L"教育漢字（小学1年）\nJIS第1水準  1面42区59点\n部首：廾（にじゅうあし）\n5画\n異体字：辨 辯 瓣",
        ChmCandidateTooltip::Body(info));
}

TEST(CandidateTooltipTest, HandlesSupplementaryAndInvalidCodePoints)
{
    EXPECT_EQ((std::wstring{wchar_t(0xD840), wchar_t(0xDC00)}), ChmCandidateTooltip::Glyph(0x20000));
    for (uint32_t cp : {0u, 0xD800u, 0xDFFFu, 0x110000u})
        EXPECT_TRUE(ChmCandidateTooltip::Glyph(cp).empty());
    ChmKanjiInfo info;
    info.variants = {{0}, {0xD800}, {0xFA55}};
    EXPECT_EQ(L"異体字：突", ChmCandidateTooltip::Body(info));
    info.variants.clear();
    info.radical.number = 116;
    EXPECT_EQ(L"部首：116", ChmCandidateTooltip::Body(info));
}

