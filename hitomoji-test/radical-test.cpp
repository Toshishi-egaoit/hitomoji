#include "gtest/gtest.h"
#include "ChmL3HelperRadical.h"
#include <set>
#include <string>

TEST(RadicalTest, CatalogIsCompleteAndReadingsAreUnique)
{
    std::set<std::wstring> readings;
    std::set<uint32_t> codePoints;
    for (uint32_t number = 1; number <= 214; ++number) {
        const auto* entry = ChmL3HelperRadical::FindByNumber(number);
        ASSERT_NE(nullptr, entry);
        EXPECT_EQ(number, entry->number);
        EXPECT_GE(entry->codePoint, 0x4E00u);
        EXPECT_LE(entry->codePoint, 0x9FFFu);
        EXPECT_TRUE(codePoints.insert(entry->codePoint).second);
        ASSERT_FALSE(entry->reading.empty());
        for (wchar_t ch : entry->reading) {
            EXPECT_GE(ch, L'\u3041');
            EXPECT_LE(ch, L'\u3096');
        }
        EXPECT_TRUE(readings.insert(std::wstring(entry->reading)).second);
        EXPECT_EQ(entry, ChmL3HelperRadical::FindByReading(entry->reading));
    }
    EXPECT_EQ(214u, readings.size());
}

TEST(RadicalTest, FindsRepresentativeRadicalsAndDistinctSimilarNames)
{
    struct Example { uint32_t number; uint32_t cp; const wchar_t* reading; };
    const Example examples[] = {
        {1, 0x4E00, L"\u3044\u3061"},
        {9, 0x4EBA, L"\u3072\u3068"},
        {64, 0x624B, L"\u3066\u3078\u3093"},
        {85, 0x6C34, L"\u3055\u3093\u305a\u3044"},
        {116, 0x7A74, L"\u3042\u306a"},
        {149, 0x8A00, L"\u3054\u3093\u3079\u3093"},
        {163, 0x9091, L"\u304a\u304a\u3056\u3068"},
        {170, 0x961C, L"\u3053\u3056\u3068\u3078\u3093"},
        {214, 0x9FA0, L"\u3084\u304f"},
    };
    for (const auto& example : examples) {
        const auto* entry = ChmL3HelperRadical::FindByReading(example.reading);
        ASSERT_NE(nullptr, entry);
        EXPECT_EQ(example.number, entry->number);
        EXPECT_EQ(example.cp, entry->codePoint);
    }
}

TEST(RadicalTest, AliasesResolveToCanonicalEntries)
{
    EXPECT_EQ(ChmL3HelperRadical::FindByNumber(9), ChmL3HelperRadical::FindByReading(L"にんべん"));
    EXPECT_EQ(ChmL3HelperRadical::FindByNumber(60), ChmL3HelperRadical::FindByReading(L"ぎょうにんべん"));
    EXPECT_EQ(ChmL3HelperRadical::FindByNumber(61), ChmL3HelperRadical::FindByReading(L"りっしんべん"));
    EXPECT_EQ(ChmL3HelperRadical::FindByNumber(61), ChmL3HelperRadical::FindByReading(L"したごころ"));
    EXPECT_EQ(ChmL3HelperRadical::FindByReading(L"さんずい"), ChmL3HelperRadical::FindByReading(L"みず"));
}

TEST(RadicalTest, RecognizesSuffixesAfterTryingTheFullReading)
{
    struct Example { uint32_t number; const wchar_t* reading; };
    const Example examples[] = {
        {113, L"しめすへん"},
        {69, L"おのつくり"},
        {69, L"おのづくり"},
        {140, L"くさかんむり"},
        {169, L"もんがまえ"},
        {153, L"むじな"},
        {60, L"ぎょうにん"},
        {94, L"けものへん"},
        {72, L"ひへん"},
        {86, L"ひ"},
        {201, L"き"},
        {163, L"おおざとへん"},
        {170, L"こざとへん"},
    };
    for (const auto& example : examples) {
        EXPECT_EQ(ChmL3HelperRadical::FindByNumber(example.number),
                  ChmL3HelperRadical::FindByReading(example.reading));
    }
    EXPECT_EQ(nullptr, ChmL3HelperRadical::FindByReading(L"もんがまえへん"));
    EXPECT_EQ(nullptr, ChmL3HelperRadical::FindByReading(L"もんかまえ"));
    EXPECT_EQ(nullptr, ChmL3HelperRadical::FindByReading(L"にん"));
    EXPECT_EQ(nullptr, ChmL3HelperRadical::FindByReading(L"しかぼね"));
    EXPECT_EQ(nullptr, ChmL3HelperRadical::FindByReading(L"へん"));
}

TEST(RadicalTest, UnknownNumbersAndReadingsAreNotMatched)
{
    for (uint32_t number : {0u, 215u, 0xFFFFFFFFu}) {
        EXPECT_EQ(nullptr, ChmL3HelperRadical::FindByNumber(number));
    }
    for (const auto reading : {L"", L"unknown", L"\u4EBA", L"\u30D2\u30C8",
        L" \u3072\u3068", L"\u3072\u3068 "}) {
        EXPECT_EQ(nullptr, ChmL3HelperRadical::FindByReading(reading));
    }
}
