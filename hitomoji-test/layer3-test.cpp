#include "gtest/gtest.h"
#include "ChmCandidatePage.h"
#include "ChmEngine.h"
#include "ChmL3Kanji.h"
#include "ChmEnvironment.h"
#include "ChmKeyEvent.h"
#include "TestConfigHelper.h"
#include "TestKeyEventHelper.h"
#include <string>
#include <sstream>

namespace {
// Build wire-format bytes explicitly, independently of the loader's C++ structs.
void PutDict32(std::string& bytes, size_t offset, uint32_t value)
{
    for (size_t i = 0; i < 4; ++i) bytes[offset + i] = static_cast<char>(value >> (8 * i));
}

std::string MakeTestDictionary(bool v2)
{
    const uint32_t headerSize = v2 ? 44 : 24;
    const uint32_t yomiOffset = headerSize + 16;
    const uint32_t listOffset = yomiOffset + 20;
    std::string bytes(listOffset + 4 + (v2 ? 15 : 0), '\0');
    PutDict32(bytes, 0, v2 ? 0x324A4D48 : 0x314A4D48);
    if (v2) {
        const uint32_t fields[] = {1, 1, 1, 1, headerSize, yomiOffset,
            listOffset, listOffset + 4, listOffset + 16, 3};
        for (size_t i = 0; i < 10; ++i) PutDict32(bytes, 4 + i * 4, fields[i]);
        PutDict32(bytes, listOffset + 4, 0x7A81);
        PutDict32(bytes, listOffset + 8, 0xFA55);
        bytes.replace(listOffset + 16, 3, "abc");
    }
    else {
        const uint32_t fields[] = {1, 1, headerSize, yomiOffset, listOffset};
        for (size_t i = 0; i < 5; ++i) PutDict32(bytes, 4 + i * 4, fields[i]);
    }
    PutDict32(bytes, headerSize, 0x7A81);
    bytes[headerSize + 8] = 116; // radical
    bytes[headerSize + 10] = 9; // strokes
    bytes[headerSize + 11] = 1; // JIS level
    bytes[headerSize + 12] = 1; // plane
    bytes[headerSize + 13] = 38; // ku
    bytes[headerSize + 14] = 45; // ten
    PutDict32(bytes, yomiOffset, 0x3068); // と
    bytes[yomiOffset + 12] = 1; // count
    PutDict32(bytes, listOffset, 0x7A81); // 突
    return bytes;
}
}

TEST(DictionaryFormatTest, LoadsBothVersionsWithoutMetadataAsCandidates)
{
    ChmL3KanjiDict dict;
    for (bool v2 : {false, true}) {
        std::istringstream input(MakeTestDictionary(v2), std::ios::binary);
        ASSERT_TRUE(dict.LoadDict(input));
        ASSERT_EQ(1u, dict.kanjiList.size());
        EXPECT_EQ(0x7A81u, dict.kanjiList[0]);
        ChmL3KanjiSelect select(&dict, 40);
        ASSERT_TRUE(select.Start(L"と"));
        EXPECT_EQ(0x7A81u, select.SelectByIndex(0));
    }
}

TEST(DictionaryFormatTest, GroupsVariantsByParentAndDisplayOrder)
{
    auto bytes = MakeTestDictionary(true);
    // Interleaved parents, unsorted orders, and a supplementary-plane code point.
    const uint32_t records[][3] = {
        {0x7A81, 0xFA55, 2}, {0x5F01, 0x8FAF, 3},
        {0x5F01, 0x8FA8, 1}, {0x7A81, 0x20000, 1},
        {0x5F01, 0x74E3, 2}, {0x7A81, 0x20001, 2}
    };
    constexpr size_t count = sizeof(records) / sizeof(records[0]);
    constexpr size_t variantOffset = 84;
    const size_t infoOffset = variantOffset + count * 12;
    bytes.resize(infoOffset + 3, '\0');
    PutDict32(bytes, 16, static_cast<uint32_t>(count));
    PutDict32(bytes, 36, static_cast<uint32_t>(infoOffset));
    for (size_t i = 0; i < count; ++i) {
        PutDict32(bytes, variantOffset + i * 12, records[i][0]);
        PutDict32(bytes, variantOffset + i * 12 + 4, records[i][1]);
        PutDict32(bytes, variantOffset + i * 12 + 8, records[i][2]);
    }
    bytes.replace(infoOffset, 3, "abc");
    ChmL3KanjiDict dict;
    std::istringstream input(bytes, std::ios::binary);
    ASSERT_TRUE(dict.LoadDict(input));
    ASSERT_EQ(2u, dict.DictVariant.size());
    EXPECT_EQ(0x5F01u, dict.DictVariant[0].parent_cp);
    EXPECT_EQ((std::vector<uint32_t>{0x8FA8, 0x74E3, 0x8FAF}), dict.DictVariant[0].variants);
    EXPECT_EQ(0x7A81u, dict.DictVariant[1].parent_cp);
    EXPECT_EQ((std::vector<uint32_t>{0x20000, 0xFA55, 0x20001}), dict.DictVariant[1].variants);
    EXPECT_EQ((std::vector<uint32_t>{0x7A81}), dict.kanjiList);
    ChmConfig config;
    ChmL3Helper helper(&config);
    ChmL3KanjiSelect select(&dict, static_cast<byte>(helper.GetPageSize()));
    ASSERT_TRUE(select.Start(L"と"));
    ChmCandidatePage page;
    ASSERT_TRUE(select.BuildCandidatePage(page, helper));
    ASSERT_EQ(1u, page.kanjiInfo.size());
    ASSERT_EQ(3u, page.kanjiInfo[0].variants.size());
    auto copiedPage = page;
    page.kanjiInfo[0].variants.clear();
    EXPECT_EQ(0x20000u, copiedPage.kanjiInfo[0].variants[0].codePoint);
    EXPECT_EQ(0xFA55u, copiedPage.kanjiInfo[0].variants[1].codePoint);
    EXPECT_EQ(0x20001u, copiedPage.kanjiInfo[0].variants[2].codePoint);

    std::istringstream single(MakeTestDictionary(true), std::ios::binary);
    ASSERT_TRUE(dict.LoadDict(single));
    ASSERT_EQ(1u, dict.DictVariant.size());
    EXPECT_EQ((std::vector<uint32_t>{0xFA55}), dict.DictVariant[0].variants);
    std::istringstream legacy(MakeTestDictionary(false), std::ios::binary);
    ASSERT_TRUE(dict.LoadDict(legacy));
    EXPECT_TRUE(dict.DictVariant.empty());
}

TEST(DictionaryFormatTest, LoadsKanjiGradesAndBuildsDisplayInformation)
{
    const std::vector<std::pair<uint16_t, ChmKanjiCategory>> grades = {
        {1, ChmKanjiCategory::Education1}, {2, ChmKanjiCategory::Education2},
        {3, ChmKanjiCategory::Education3}, {4, ChmKanjiCategory::Education4},
        {5, ChmKanjiCategory::Education5}, {6, ChmKanjiCategory::Education6},
        {8, ChmKanjiCategory::Joyo}, {9, ChmKanjiCategory::Jinmeiyo},
        {10, ChmKanjiCategory::Jinmeiyo}, {0, ChmKanjiCategory::Other},
        {7, ChmKanjiCategory::Other}, {255, ChmKanjiCategory::Other}
    };
    ChmL3KanjiDict dict;
    ChmConfig config;
    ChmL3Helper helper(&config);
    for (bool v2 : {false, true}) {
        for (const auto& grade : grades) {
            auto bytes = MakeTestDictionary(v2);
            const size_t headerSize = v2 ? 44 : 24;
            PutDict32(bytes, headerSize + 4, 123u);
            bytes[headerSize + 9] = static_cast<char>(grade.first);
            std::istringstream input(bytes, std::ios::binary);
            ASSERT_TRUE(dict.LoadDict(input));
            ASSERT_EQ(1u, dict.DictKanji.size());
            EXPECT_EQ(0x7A81u, dict.DictKanji[0].codepoint);
            EXPECT_EQ(grade.first, dict.DictKanji[0].grade);
            EXPECT_EQ(123u, dict.DictKanji[0].freq);
            ChmL3KanjiSelect select(&dict, static_cast<byte>(helper.GetPageSize()));
            ASSERT_TRUE(select.Start(L"と"));
            ChmCandidatePage page;
            ASSERT_TRUE(select.BuildCandidatePage(page, helper));
            ASSERT_EQ(1u, page.kanjiInfo.size());
            const auto& info = page.kanjiInfo[0];
            EXPECT_EQ(static_cast<size_t>(helper.IndexToCell(0)), info.cellIndex);
            EXPECT_EQ(info.codePoint, page.candidates[info.cellIndex]);
            EXPECT_EQ(grade.second, info.category);
            EXPECT_EQ(1u, info.jis.plane);
            EXPECT_EQ(1u, info.jis.level);
            EXPECT_EQ(0x7A74u, info.radical.codePoint); // 穴
            EXPECT_EQ(L"あな", info.radical.name);
            EXPECT_EQ(9u, info.strokeCount);
            EXPECT_EQ(116u, info.radical.number);
            EXPECT_EQ(38u, info.jis.ku);
            EXPECT_EQ(45u, info.jis.ten);
            EXPECT_EQ(v2 ? 1u : 0u, info.variants.size());
            auto copiedPage = page;
            page.kanjiInfo.clear();
            ASSERT_EQ(1u, copiedPage.kanjiInfo.size());
            select.Cancel();
            EXPECT_FALSE(select.BuildCandidatePage(copiedPage, helper));
            EXPECT_TRUE(copiedPage.kanjiInfo.empty());
        }
    }
    EXPECT_EQ(ChmKanjiCategory::Unknown, dict.GetKanjiInfo(0x4E00).category);
}
TEST(DictionaryFormatTest, PageTotalsIncludeEmptyPagesAndRespectPageSize)
{
    ChmL3KanjiDict dict;
    dict.DictYomi = { { {u'と', 0}, 81, 0 } };
    dict.kanjiList.assign(81, 0);
    dict.kanjiList.back() = 0x7A81;
    ChmConfig config;
    ChmL3Helper helper(&config);
    for (byte pageSize : {byte(34), byte(40)}) {
        ChmL3KanjiSelect select(&dict, pageSize);
        ASSERT_TRUE(select.Start(L"と"));
        ChmCandidatePage page;
        for (UINT index = 0; index < 3; ++index) {
            ASSERT_TRUE(select.BuildCandidatePage(page, helper));
            EXPECT_EQ(index, page.page);
            EXPECT_EQ(3u, page.totalPages);
            if (index < 2) {
                for (auto cp : page.candidates) EXPECT_EQ(0u, cp);
            }
            select.NextPage();
        }
        ASSERT_TRUE(select.BuildCandidatePage(page, helper));
        EXPECT_EQ(0u, page.page);
        EXPECT_EQ(3u, page.totalPages);
        select.Cancel();
        EXPECT_FALSE(select.BuildCandidatePage(page, helper));
        EXPECT_EQ(0u, page.totalPages);
    }
    ChmL3KanjiSelect select(&dict, 1);
    ASSERT_TRUE(select.Start(L"と"));
    ChmCandidatePage page;
    ASSERT_TRUE(select.BuildCandidatePage(page, helper));
    EXPECT_EQ(81u, page.totalPages);
}

TEST(DictionaryFormatTest, SupportsAbsentOptionalBlocks)
{
    auto bytes = MakeTestDictionary(true);
    bytes.resize(84);
    for (size_t offset : {4u, 16u, 32u, 36u, 40u}) PutDict32(bytes, offset, 0);
    ChmL3KanjiDict dict;
    std::istringstream input(bytes, std::ios::binary);
    ASSERT_TRUE(dict.LoadDict(input));
    EXPECT_EQ(1u, dict.kanjiList.size());
    EXPECT_TRUE(dict.DictVariant.empty());
}

TEST(DictionaryFormatTest, RejectsInvalidRangesAndClearsPreviousData)
{
    const auto valid = MakeTestDictionary(true);
    std::vector<std::string> invalid;
    for (size_t size : {0u, 4u, 43u, 90u}) invalid.push_back(valid.substr(0, size));
    for (const auto& change : std::vector<std::pair<size_t, uint32_t>>{
        {0, 0}, {8, 0xFFFFFFFF}, {12, 0xFFFFFFFF}, {16, 0xFFFFFFFF},
        {20, 0}, {24, 0}, {28, 0xFFFFFFFF}, {32, 81}, {36, 85}, {40, 100},
        {76, 1}}) { // yomi offset beyond its single candidate
        auto bytes = valid;
        PutDict32(bytes, change.first, change.second);
        invalid.push_back(bytes);
    }
    ChmL3KanjiDict dict;
    for (const auto& bytes : invalid) {
        std::istringstream good(valid, std::ios::binary);
        ASSERT_TRUE(dict.LoadDict(good));
        std::istringstream bad(bytes, std::ios::binary);
        EXPECT_FALSE(dict.LoadDict(bad));
        EXPECT_TRUE(dict.DictKanji.empty());
        EXPECT_TRUE(dict.DictVariant.empty());
        EXPECT_TRUE(dict.DictYomi.empty());
        EXPECT_TRUE(dict.kanjiList.empty());
    }
}

namespace {

bool FileExists(const std::wstring& path)
{
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring GetFullPath(const std::wstring& relativePath)
{
    wchar_t fullPath[MAX_PATH];
    DWORD length = GetFullPathNameW(relativePath.c_str(), MAX_PATH, fullPath, nullptr);
    if (length == 0 || length >= MAX_PATH) return L"";
    return fullPath;
}

std::wstring FindDistributionBasePath()
{
    const wchar_t* candidates[] = {
        L"distribution\\",
        L"..\\distribution\\",
        L"..\\..\\distribution\\",
        L"..\\..\\..\\distribution\\",
        L"..\\..\\..\\..\\distribution\\",
    };

    for (const wchar_t* candidate : candidates) {
        std::wstring basePath = GetFullPath(candidate);
        if (!basePath.empty() && FileExists(basePath + L"hitomoji.dic")) {
            return basePath;
        }
    }
    return L"";
}

class Layer3Test : public ::testing::Test {
protected:
    void SetUp() override
    {
        g_environment.Init();
        _oldBasePath = g_environment.GetBasePath();
        _distributionBasePath = FindDistributionBasePath();
        ASSERT_FALSE(_distributionBasePath.empty());
        ASSERT_TRUE(FileExists(_distributionBasePath + L"hitomoji.dic"));
        _dir.WriteFile(L"Hitomoji.ini",
            L"[ui]\n"
            L"select_keymap=dk fj sl gh ei ru a; wo qp ty c, vm x. z/ bn 38 47 29 10 56\n"
            L"[key-table]\n"
            L"->=→\n"
            L"<<=《\n");
        _dir.CopyFileFrom(_distributionBasePath + L"hitomoji.dic", L"hitomoji.dic");
        g_environment.SetBasePath(_dir.BaseDir());

        TestKeyEventHelper::Install();
        ChmKeyEvent::InitFunctionKey();
    }

    void TearDown() override
    {
        ChmKeyEvent::InitFunctionKey();
        TestKeyEventHelper::Restore();
        g_environment.SetBasePath(_oldBasePath);
    }

    ChmKeyEvent MakeKey(WPARAM key,
                        ChmEngine::State state,
                        bool shift = false,
                        bool ctrl = false,
                        bool alt = false,
                        bool caps = false)
    {
        TestKeyEventHelper::SetState(shift, ctrl, alt, caps);
        return ChmKeyEvent(key, 0, state);
    }

    void WriteMainConfig(const std::wstring& content)
    {
        _dir.WriteFile(L"Hitomoji.ini", content);
    }

    void InputKeys(ChmEngine& engine, const char* keys, const wchar_t* composition)
    {
        for (const char* p = keys; *p != '\0'; ++p) {
            bool endComposition = true;
            ChmKeyEvent keyEvent = MakeKey(static_cast<unsigned char>(*p), engine.GetState());

            EXPECT_EQ(ChmFuncType::CharInput, keyEvent.GetType());
            EXPECT_TRUE(engine.UpdateComposition(keyEvent, endComposition));
            EXPECT_FALSE(endComposition);
            EXPECT_EQ(ChmEngine::State::Inputing, engine.GetState());
        }
        EXPECT_EQ(composition, engine.GetCompositionStr());
    }

    void InputKey(ChmEngine& engine, WPARAM key, const wchar_t* composition,
                  bool shift = false)
    {
        bool endComposition = true;
        ChmKeyEvent keyEvent = MakeKey(key, engine.GetState(), shift);

        EXPECT_EQ(ChmFuncType::CharInput, keyEvent.GetType());
        EXPECT_TRUE(engine.UpdateComposition(keyEvent, endComposition));
        EXPECT_FALSE(endComposition);
        EXPECT_EQ(ChmEngine::State::Inputing, engine.GetState());
        EXPECT_EQ(composition, engine.GetCompositionStr());
    }

    void StartSelection(ChmEngine& engine, ChmCandidatePage& page)
    {
        bool endComposition = true;
        ChmKeyEvent selectKey = MakeKey(VK_SPACE, engine.GetState());

        EXPECT_EQ(ChmFuncType::CompSelect, selectKey.GetType());
        EXPECT_TRUE(engine.UpdateComposition(selectKey, endComposition));
        EXPECT_FALSE(endComposition);
        EXPECT_EQ(ChmEngine::State::Selecting, engine.GetState());
        EXPECT_TRUE(engine.HasComposition());
        EXPECT_TRUE(engine.GetCandidatePage(page));
    }

    void SelectNextPage(ChmEngine& engine, ChmCandidatePage& page)
    {
        bool endComposition = true;
        ChmKeyEvent nextPageKey = MakeKey(VK_SPACE, engine.GetState());

        EXPECT_EQ(ChmFuncType::SelectNextPage, nextPageKey.GetType());
        EXPECT_TRUE(engine.UpdateLayer3(nextPageKey, endComposition, &page));
        EXPECT_FALSE(endComposition);
        EXPECT_EQ(ChmEngine::State::Selecting, engine.GetState());
        EXPECT_TRUE(engine.HasComposition());
    }

    void SelectPrevPage(ChmEngine& engine, ChmCandidatePage& page)
    {
        bool endComposition = true;
        ChmKeyEvent prevPageKey = MakeKey(VK_BACK, engine.GetState());

        EXPECT_EQ(ChmFuncType::SelectPrevPage, prevPageKey.GetType());
        EXPECT_TRUE(engine.UpdateLayer3(prevPageKey, endComposition, &page));
        EXPECT_FALSE(endComposition);
        EXPECT_EQ(ChmEngine::State::Selecting, engine.GetState());
        EXPECT_TRUE(engine.HasComposition());
    }

    void CancelSelection(ChmEngine& engine, const wchar_t* composition)
    {
        bool endComposition = true;
        ChmKeyEvent cancelKey = MakeKey(VK_ESCAPE, engine.GetState());

        EXPECT_EQ(ChmFuncType::SelectCancel, cancelKey.GetType());
        EXPECT_TRUE(engine.UpdateComposition(cancelKey, endComposition));
        EXPECT_FALSE(endComposition);
        EXPECT_EQ(ChmEngine::State::Inputing, engine.GetState());
        EXPECT_TRUE(engine.HasComposition());
        EXPECT_EQ(composition, engine.GetCompositionStr());

        ChmCandidatePage page;
        EXPECT_FALSE(engine.GetCandidatePage(page));
    }

    void SelectCandidate(ChmEngine& engine, WPARAM key, const wchar_t* composition,
                         bool shift = false)
    {
        bool endComposition = false;
        ChmKeyEvent selectKey = MakeKey(key, engine.GetState(), shift);

        EXPECT_EQ(ChmFuncType::SelectInput, selectKey.GetType());
        EXPECT_TRUE(engine.UpdateComposition(selectKey, endComposition));
        EXPECT_TRUE(endComposition);
        EXPECT_EQ(ChmEngine::State::None, engine.GetState());
        EXPECT_FALSE(engine.HasComposition());
        EXPECT_EQ(composition, engine.GetCompositionStr());

        ChmCandidatePage page;
        EXPECT_FALSE(engine.GetCandidatePage(page));
    }

    std::wstring _oldBasePath;
    std::wstring _distributionBasePath;
    TempConfigDir _dir;
};

} // namespace

TEST_F(Layer3Test, ConvertKeyStartsKanjiSelectionAndBuildsCandidatePageForKou)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "KOU", L"こう");

    ChmCandidatePage page;
    StartSelection(engine, page);
    EXPECT_EQ(L"こう", engine.GetCompositionStr());

    EXPECT_EQ(0u, page.page);
    // HMJ2: 131 candidates minus 8 accepted variants (indices 0 through 122).
    EXPECT_EQ(123u, page.totalCount);
    EXPECT_EQ(40u, page.candidateCount);

    engine.Deactivate();
}

TEST_F(Layer3Test, ConvertKeyFiltersKanjiSelectionByAdditionalYomiOperator)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "KOU", L"\x3053\x3046");
    InputKey(engine, VK_OEM_1, L"\x3053\x3046:", true);
	InputKeys(engine, "KI", L"\x3053\x3046:\x304D");

	ChmCandidatePage page;
    StartSelection(engine, page);
    EXPECT_EQ(L"\x3053\x3046:\x304D", engine.GetCompositionStr());

    EXPECT_GT(page.totalCount, 20u);
    EXPECT_EQ(40u, page.candidateCount);

    bool hasKouKi = false;
    bool hasFilteredSlot = false;
    for (uint32_t candidate : page.candidates) {
        if (candidate == 0x52B9u) {
            hasKouKi = true;
        }
        else if (candidate == 0) {
            hasFilteredSlot = true;
        }
    }
    EXPECT_TRUE(hasKouKi);
    EXPECT_TRUE(hasFilteredSlot);

    engine.Deactivate();
}

TEST_F(Layer3Test, ConvertKeyKeepsInputingAndRequestsErrorWhenYomiDoesNotExist)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "NN", L"ん");

    bool endComposition = true;
    ChmKeyEvent selectKey = MakeKey(VK_SPACE, engine.GetState());

    EXPECT_EQ(ChmFuncType::CompSelect, selectKey.GetType());
    EXPECT_TRUE(engine.UpdateComposition(selectKey, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_TRUE(engine.ConsumeErrorRequest());
    EXPECT_EQ(ChmEngine::State::Inputing, engine.GetState());
    EXPECT_TRUE(engine.HasComposition());
    EXPECT_EQ(L"ん", engine.GetCompositionStr());

    ChmCandidatePage page;
    EXPECT_FALSE(engine.GetCandidatePage(page));

    engine.Deactivate();
}

TEST_F(Layer3Test, ConvertKeyKeepsInputingAndRequestsErrorWhenDictionaryIsMissing)
{
    ASSERT_TRUE(DeleteFileW((_dir.BaseDir() + L"hitomoji.dic").c_str()));

    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "KOU", L"こう");

    bool endComposition = true;
    ChmKeyEvent selectKey = MakeKey(VK_SPACE, engine.GetState());

    EXPECT_EQ(ChmFuncType::CompSelect, selectKey.GetType());
    EXPECT_TRUE(engine.UpdateComposition(selectKey, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_TRUE(engine.ConsumeErrorRequest());
    EXPECT_EQ(ChmEngine::State::Inputing, engine.GetState());
    EXPECT_TRUE(engine.HasComposition());
    EXPECT_EQ(L"こう", engine.GetCompositionStr());

    ChmCandidatePage page;
    EXPECT_FALSE(engine.GetCandidatePage(page));

    engine.Deactivate();
}

TEST_F(Layer3Test, ConvertKeyFindsFirstYomiEntryAndPreservesSparseSlots)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "TAT", L"たt");
	// この読みの漢字は、「達」「宅」「卓」の3種

    ChmCandidatePage page;
    StartSelection(engine, page);

    EXPECT_EQ(0u, page.page);
	EXPECT_GT(page.totalCount,3u);
    EXPECT_EQ(6u, page.candidateCount);
    EXPECT_EQ(0u, page.candidates[22]);        // at "D" index 0:  わりあてなし
    EXPECT_EQ(0x9054u, page.candidates[26]);   // at "J" index 3:  達
    EXPECT_EQ(0u, page.candidates[8]);        //  at "9" index -: ここは使用不可なので常に空欄

    engine.Deactivate();
}

TEST_F(Layer3Test, ConvertKeyFindsLastYomiEntryAndPreservesSparseSlots)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "WO", L"を");

    ChmCandidatePage page;
    StartSelection(engine, page);

    EXPECT_EQ(0u, page.page);
    EXPECT_EQ(65u, page.totalCount);
    EXPECT_EQ(40u, page.candidateCount);
    EXPECT_EQ(0u, page.candidates[22]);

    SelectNextPage(engine, page);

    EXPECT_EQ(1u, page.page);
    EXPECT_EQ(65u, page.totalCount);
    EXPECT_EQ(25u, page.candidateCount);
    EXPECT_EQ(0x4E4Eu, page.candidates[31]); // index 64: 乎

    engine.Deactivate();
}

TEST_F(Layer3Test, ConvertKeyIgnoresLeadingSymbolsWhenSearchingYomi)
{
    ChmEngine engine;
    engine.Activate();

    bool endComposition = true;
    ChmKeyEvent minusKey = MakeKey(VK_OEM_MINUS, engine.GetState());
    EXPECT_EQ(ChmFuncType::CharInput, minusKey.GetType());
    EXPECT_TRUE(engine.UpdateComposition(minusKey, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_EQ(L"ー", engine.GetCompositionStr());

    ChmKeyEvent greaterKey = MakeKey(VK_OEM_PERIOD, engine.GetState(), true);
    EXPECT_EQ(ChmFuncType::CharInput, greaterKey.GetType());
    EXPECT_TRUE(engine.UpdateComposition(greaterKey, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_EQ(L"→", engine.GetCompositionStr());

    InputKeys(engine, "AI", L"→あい");

    ChmCandidatePage page;
    StartSelection(engine, page);
    EXPECT_EQ(L"→あい", engine.GetCompositionStr());

    EXPECT_EQ(0u, page.page);
    EXPECT_EQ(26u, page.totalCount);
    EXPECT_EQ(26u, page.candidateCount);

    engine.Deactivate();
}

TEST_F(Layer3Test, ConvertKeyIgnoresMultipleDifferentLeadingSymbolsWhenSearchingYomi)
{
    ChmEngine engine;
    engine.Activate();

    bool endComposition = true;
    ChmKeyEvent minusKey = MakeKey(VK_OEM_MINUS, engine.GetState());
    EXPECT_EQ(ChmFuncType::CharInput, minusKey.GetType());
    EXPECT_TRUE(engine.UpdateComposition(minusKey, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_EQ(L"ー", engine.GetCompositionStr());

    ChmKeyEvent greaterKey = MakeKey(VK_OEM_PERIOD, engine.GetState(), true);
    EXPECT_EQ(ChmFuncType::CharInput, greaterKey.GetType());
    EXPECT_TRUE(engine.UpdateComposition(greaterKey, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_EQ(L"→", engine.GetCompositionStr());

    ChmKeyEvent lessKey1 = MakeKey(VK_OEM_COMMA, engine.GetState(), true);
    EXPECT_EQ(ChmFuncType::CharInput, lessKey1.GetType());
    EXPECT_TRUE(engine.UpdateComposition(lessKey1, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_EQ(L"→<", engine.GetCompositionStr());

    ChmKeyEvent lessKey2 = MakeKey(VK_OEM_COMMA, engine.GetState(), true);
    EXPECT_EQ(ChmFuncType::CharInput, lessKey2.GetType());
    EXPECT_TRUE(engine.UpdateComposition(lessKey2, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_EQ(L"→《", engine.GetCompositionStr());

    InputKeys(engine, "AI", L"→《あい");

    ChmCandidatePage page;
    StartSelection(engine, page);
    EXPECT_EQ(L"→《あい", engine.GetCompositionStr());

    EXPECT_EQ(0u, page.page);
    EXPECT_EQ(26u, page.totalCount);
    EXPECT_EQ(26u, page.candidateCount);

    engine.Deactivate();
}

TEST_F(Layer3Test, ConvertKeyIgnoresLeadingNonHiraganaWhenSearchingYomi)
{
    WriteMainConfig(
        L"[ui]\n"
        L"select_keymap=dk fj sl gh ei ru a; wo qp ty c, vm x. z/ bn 38 47 29 10 56\n"
        L"[key-table]\n"
        L"qq=字\n");

    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "QQAI", L"字あい");

    ChmCandidatePage page;
    StartSelection(engine, page);
    EXPECT_EQ(L"字あい", engine.GetCompositionStr());

    EXPECT_EQ(0u, page.page);
    EXPECT_EQ(26u, page.totalCount);
    EXPECT_EQ(26u, page.candidateCount);

    SelectCandidate(engine, 'D', L"字会");

    engine.Deactivate();
}

TEST_F(Layer3Test, ConvertKeyKeepsInputingAndRequestsErrorWhenPendingRemains)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "AIY", L"あいy");

    bool endComposition = true;
    ChmKeyEvent selectKey = MakeKey(VK_SPACE, engine.GetState());

    EXPECT_EQ(ChmFuncType::CompSelect, selectKey.GetType());
    EXPECT_TRUE(engine.UpdateComposition(selectKey, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_TRUE(engine.ConsumeErrorRequest());
    EXPECT_EQ(ChmEngine::State::Inputing, engine.GetState());
    EXPECT_TRUE(engine.HasComposition());
    EXPECT_EQ(L"あいy", engine.GetCompositionStr());

    ChmCandidatePage page;
    EXPECT_FALSE(engine.GetCandidatePage(page));

    engine.Deactivate();
}

TEST_F(Layer3Test, ConvertKeyCompletesSokuonPendingBeforeSearchingYomi)
{
    struct Case {
        const char* input;
        const wchar_t* composition;
    };
    const Case cases[] = {
        { "AK", L"あk" },
        { "AS", L"あs" },
        { "AT", L"あt" },
        { "AP", L"あp" },
    };

    for (const Case& c : cases) {
        SCOPED_TRACE(c.input);

        ChmEngine engine;
        engine.Activate();

        InputKeys(engine, c.input, c.composition);

        ChmCandidatePage page;
        StartSelection(engine, page);
        EXPECT_EQ(L"あっ", engine.GetCompositionStr());

        EXPECT_EQ(0u, page.page);
        EXPECT_EQ(43u, page.totalCount);
        EXPECT_EQ(40u, page.candidateCount);

        engine.Deactivate();
    }
}

TEST_F(Layer3Test, BackspaceAfterSokuonPendingSelectionRestoresPreviousComposition)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "RAT", L"らt");
	// この読みの漢字は、「拉」「楽」「落」の3種

    ChmCandidatePage page;
    StartSelection(engine, page);
    EXPECT_EQ(L"らっ", engine.GetCompositionStr());
	EXPECT_GE(page.totalCount, 3u);
	EXPECT_GE(page.candidateCount, 3u);

    bool endComposition = true;
    ChmKeyEvent backspaceKey = MakeKey(VK_BACK, engine.GetState());

    EXPECT_EQ(ChmFuncType::SelectPrevPage, backspaceKey.GetType());
    EXPECT_TRUE(engine.UpdateComposition(backspaceKey, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_EQ(ChmEngine::State::Inputing, engine.GetState());
    EXPECT_TRUE(engine.HasComposition());
    EXPECT_EQ(L"ら", engine.GetCompositionStr());
    EXPECT_FALSE(engine.GetCandidatePage(page));

    engine.Deactivate();
}

TEST_F(Layer3Test, EscapeCancelsSelectionAndRestoresInputingComposition)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "A", L"あ");

    ChmCandidatePage page;
    StartSelection(engine, page);
    EXPECT_EQ(L"あ", engine.GetCompositionStr());
    EXPECT_EQ(0u, page.page);

    SelectNextPage(engine, page);
    EXPECT_EQ(1u, page.page);

    CancelSelection(engine, L"あ");

    engine.Deactivate();
}

TEST_F(Layer3Test, EscapeCancelsSelectionWithLeadingSymbolsAndRestoresComposition)
{
    ChmEngine engine;
    engine.Activate();

    bool endComposition = true;
    ChmKeyEvent minusKey = MakeKey(VK_OEM_MINUS, engine.GetState());
    EXPECT_TRUE(engine.UpdateComposition(minusKey, endComposition));
    EXPECT_FALSE(endComposition);

    ChmKeyEvent greaterKey = MakeKey(VK_OEM_PERIOD, engine.GetState(), true);
    EXPECT_TRUE(engine.UpdateComposition(greaterKey, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_EQ(L"→", engine.GetCompositionStr());

    InputKeys(engine, "AI", L"→あい");

    ChmCandidatePage page;
    StartSelection(engine, page);
    EXPECT_EQ(L"→あい", engine.GetCompositionStr());

    CancelSelection(engine, L"→あい");

    engine.Deactivate();
}

TEST_F(Layer3Test, EscapeCancelsSelectionAfterSokuonPendingAndRestoresRawPending)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "RAT", L"らt");

    ChmCandidatePage page;
    StartSelection(engine, page);
    EXPECT_EQ(L"らっ", engine.GetCompositionStr());

    CancelSelection(engine, L"らt");

    engine.Deactivate();
}

TEST_F(Layer3Test, ColonDuringSelectionCancelsSelectionAndAppendsFilterSeparator)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "A", L"\x3042");

    ChmCandidatePage page;
    StartSelection(engine, page);
    EXPECT_EQ(L"\x3042", engine.GetCompositionStr());

    bool endComposition = true;
    ChmKeyEvent colonKey = MakeKey(VK_OEM_1, engine.GetState(), true);

    EXPECT_EQ(ChmFuncType::SelectCancelAndInput, colonKey.GetType());
    EXPECT_TRUE(engine.UpdateComposition(colonKey, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_EQ(ChmEngine::State::Inputing, engine.GetState());
    EXPECT_TRUE(engine.HasComposition());
    EXPECT_EQ(L"\x3042:", engine.GetCompositionStr());
    EXPECT_FALSE(engine.GetCandidatePage(page));

    engine.Deactivate();
}

TEST_F(Layer3Test, CustomSelectKeymapControlsCandidatePageSizeAndCells)
{
    {
        WriteMainConfig(
            L"[ui]\n"
            L"select_keymap=a/\n");

        ChmEngine engine;
        engine.Activate();

        InputKeys(engine, "A", L"あ");

        ChmCandidatePage page;
        StartSelection(engine, page);

        EXPECT_EQ(0u, page.page);
        EXPECT_GT(page.totalCount, 39u);
        EXPECT_EQ(2u, page.candidateCount);
        EXPECT_EQ(0x4F1Au, page.candidates[20]); // at "A" index 0, 会
        EXPECT_EQ(0x4E0Au, page.candidates[39]); // at "/" index 1, 上

        SelectNextPage(engine, page);
        EXPECT_EQ(1u, page.page);
        EXPECT_GT(page.totalCount, 39u);
        EXPECT_EQ(2u, page.candidateCount);
        EXPECT_EQ(0x5F53u, page.candidates[20]); // at "A" index 2, 当
        EXPECT_EQ(0x4E2Du, page.candidates[39]); // at "/" index 3, 中

        engine.Deactivate();
    }
    {
        WriteMainConfig(
            L"[ui]\n"
            L"select_keymap=a/;\n");

        ChmEngine engine;
        engine.Activate();

        InputKeys(engine, "A", L"あ");

        ChmCandidatePage page;
        StartSelection(engine, page);

        EXPECT_EQ(0u, page.page);
        EXPECT_GT(page.totalCount, 39u);
        EXPECT_EQ(3u, page.candidateCount);
        EXPECT_EQ(0x4F1Au, page.candidates[20]); // at "A" index 0, 会
        EXPECT_EQ(0x4E0Au, page.candidates[39]); // at "/" index 1, 上
        EXPECT_EQ(0x5F53u, page.candidates[29]); // at ";" index 2, 当

        engine.Deactivate();
    }
}

TEST_F(Layer3Test, SelectInputUsesCustomSelectKeymapIncludingOemKeys)
{
    WriteMainConfig(
        L"[ui]\n"
        L"select_keymap=a/;\n");

    {
        ChmEngine engine;
        engine.Activate();

        InputKeys(engine, "A", L"あ");

        ChmCandidatePage page;
        StartSelection(engine, page);

        SelectCandidate(engine, 'A', L"会");

        engine.Deactivate();
    }
    {
        ChmEngine engine;
        engine.Activate();

        InputKeys(engine, "A", L"あ");

        ChmCandidatePage page;
        StartSelection(engine, page);

        SelectCandidate(engine, VK_OEM_2, L"上");

        engine.Deactivate();
    }
    {
        ChmEngine engine;
        engine.Activate();

        InputKeys(engine, "A", L"あ");

        ChmCandidatePage page;
        StartSelection(engine, page);

        SelectCandidate(engine, VK_OEM_1, L"当");

        engine.Deactivate();
    }
}

TEST_F(Layer3Test, SelectInputPreservesLeadingSymbolsInCommittedComposition)
{
    ChmEngine engine;
    engine.Activate();

    bool endComposition = true;
    ChmKeyEvent minusKey = MakeKey(VK_OEM_MINUS, engine.GetState());
    EXPECT_TRUE(engine.UpdateComposition(minusKey, endComposition));
    EXPECT_FALSE(endComposition);

    ChmKeyEvent greaterKey = MakeKey(VK_OEM_PERIOD, engine.GetState(), true);
    EXPECT_TRUE(engine.UpdateComposition(greaterKey, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_EQ(L"→", engine.GetCompositionStr());

    InputKeys(engine, "AI", L"→あい");

    ChmCandidatePage page;
    StartSelection(engine, page);
    EXPECT_EQ(L"→あい", engine.GetCompositionStr());
    ASSERT_EQ(26u, page.totalCount);
    ASSERT_EQ(26u, page.candidateCount);

    SelectCandidate(engine, 'D', L"→会");

    engine.Deactivate();
}

TEST_F(Layer3Test, SelectInputKeepsSelectingAndRequestsErrorForEmptySlot)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "KET", L"けt");

    ChmCandidatePage page;
    StartSelection(engine, page);

    ASSERT_EQ(0u, page.page);
    ASSERT_EQ(page.totalCount, 32u);
    ASSERT_EQ(32u, page.totalCount);
    ASSERT_EQ(32u, page.candidateCount);
    ASSERT_EQ(0u, page.candidates[2]); // 3: index 30, missing slot

    bool endComposition = true;
    ChmKeyEvent missingSlotKey = MakeKey('3', engine.GetState());

    EXPECT_EQ(ChmFuncType::SelectInput, missingSlotKey.GetType());
    EXPECT_TRUE(engine.UpdateComposition(missingSlotKey, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_TRUE(engine.ConsumeErrorRequest());
    EXPECT_EQ(ChmEngine::State::Selecting, engine.GetState());
    EXPECT_TRUE(engine.HasComposition());
    EXPECT_EQ(L"けっ", engine.GetCompositionStr());
    EXPECT_TRUE(engine.GetCandidatePage(page));
    EXPECT_EQ(0u, page.page);

    engine.Deactivate();
}

TEST_F(Layer3Test, SelectInputUsesThirtyFourKeymapAcrossPages)
{
    WriteMainConfig(
        L"[ui]\n"
        L"select_keymap=dk fj sl gh ei ru a; wo qp ty c, vm x. z/ bn 34 89\n");

    {
        ChmEngine engine;
        engine.Activate();

        InputKeys(engine, "KOU", L"こう");

        ChmCandidatePage page;
        StartSelection(engine, page);

        EXPECT_EQ(0u, page.page);
        EXPECT_EQ(123u, page.totalCount);
        EXPECT_EQ(34u, page.candidateCount);
        EXPECT_EQ(0x63A7u, page.candidates[8]); // 9: index 33, 控

        SelectCandidate(engine, '9', L"控");

        engine.Deactivate();
    }
    {
        ChmEngine engine;
        engine.Activate();

        InputKeys(engine, "KOU", L"こう");

        ChmCandidatePage page;
        StartSelection(engine, page);
        SelectNextPage(engine, page);

        EXPECT_EQ(1u, page.page);
        EXPECT_EQ(123u, page.totalCount);
        EXPECT_EQ(34u, page.candidateCount);
        EXPECT_EQ(0x9271u, page.candidates[8]); // 9: index 67, 鉱

        SelectCandidate(engine, '9', L"鉱");

        engine.Deactivate();
    }
}

TEST_F(Layer3Test, SameSelectKeyChoosesDifferentKanjiWhenKeymapChanges)
{
    {
        WriteMainConfig(
            L"[ui]\n"
            L"select_keymap=dk fj sl gh ei ru a; wo qp ty c, vm x. z/ bn 34 89\n");

        ChmEngine engine;
        engine.Activate();

        InputKeys(engine, "KOU", L"こう");

        ChmCandidatePage page;
        StartSelection(engine, page);

        EXPECT_EQ(34u, page.candidateCount);
        EXPECT_EQ(0x63A7u, page.candidates[8]); // 9: index 33, 控

        SelectCandidate(engine, '9', L"控");

        engine.Deactivate();
    }
    {
        WriteMainConfig(
            L"[ui]\n"
            L"select_keymap=dk fj sl gh ei ru a; wo qp ty c, vm x. z/ bn 38 47 29 10 56\n");

        ChmEngine engine;
        engine.Activate();

        InputKeys(engine, "KOU", L"こう");

        ChmCandidatePage page;
        StartSelection(engine, page);

        EXPECT_EQ(40u, page.candidateCount);
        EXPECT_EQ(0x6297u, page.candidates[8]); // 9: index 35, 抗

        SelectCandidate(engine, '9', L"抗");

        engine.Deactivate();
    }
}

TEST_F(Layer3Test, SelectNextPageDoesNotMoveWhenOnlyOnePageExists)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "AI", L"あい");

    ChmCandidatePage page;
    StartSelection(engine, page);

    EXPECT_EQ(0u, page.page);
    EXPECT_EQ(26u, page.totalCount);
    EXPECT_EQ(26u, page.candidateCount);

    SelectNextPage(engine, page);

    EXPECT_EQ(0u, page.page);
    EXPECT_EQ(26u, page.totalCount);
    EXPECT_EQ(26u, page.candidateCount);
    EXPECT_FALSE(engine.ConsumeErrorRequest());

    engine.Deactivate();
}

TEST_F(Layer3Test, SelectNextPageMovesForwardAcrossMultiplePages)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "A", L"あ");

    ChmCandidatePage page;
    StartSelection(engine, page);

    EXPECT_EQ(0u, page.page);
    EXPECT_EQ(87u, page.totalCount);
    EXPECT_EQ(40u, page.candidateCount);

    SelectNextPage(engine, page);
    EXPECT_EQ(1u, page.page);
    EXPECT_EQ(87u, page.totalCount);
    EXPECT_EQ(40u, page.candidateCount);

    SelectNextPage(engine, page);
    EXPECT_EQ(2u, page.page);
    EXPECT_EQ(87u, page.totalCount);
    EXPECT_EQ(7u, page.candidateCount);
    EXPECT_FALSE(engine.ConsumeErrorRequest());

    engine.Deactivate();
}

TEST_F(Layer3Test, SelectNextPageWrapsToFirstPageAfterLastPage)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "A", L"あ");

    ChmCandidatePage page;
    StartSelection(engine, page);
    SelectNextPage(engine, page);
    SelectNextPage(engine, page);

    ASSERT_EQ(2u, page.page);
    ASSERT_EQ(7u, page.candidateCount);

	SelectNextPage(engine, page);

    EXPECT_EQ(0u, page.page);
	EXPECT_EQ(87u, page.totalCount);
	EXPECT_EQ(40u, page.candidateCount);
	EXPECT_FALSE(engine.ConsumeErrorRequest());

    engine.Deactivate();
}

TEST_F(Layer3Test, SelectPrevPageMovesBackFromSecondPage)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "WO", L"を");

    ChmCandidatePage page;
    StartSelection(engine, page);
    SelectNextPage(engine, page);

    ASSERT_EQ(1u, page.page);
    ASSERT_EQ(25u, page.candidateCount);

    SelectPrevPage(engine, page);

    EXPECT_EQ(0u, page.page);
    EXPECT_EQ(65u, page.totalCount);
    EXPECT_EQ(40u, page.candidateCount);
    EXPECT_FALSE(engine.ConsumeErrorRequest());

    engine.Deactivate();
}

TEST_F(Layer3Test, SelectPrevPageWrapsToLastPageBeforeFirstPage)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "WO", L"を");

    ChmCandidatePage page;
    StartSelection(engine, page);

    ASSERT_EQ(0u, page.page);
    ASSERT_EQ(40u, page.candidateCount);

	SelectPrevPage(engine, page);

    EXPECT_EQ(1u, page.page);
	EXPECT_EQ(65u, page.totalCount);
	EXPECT_EQ(25u, page.candidateCount);
	EXPECT_FALSE(engine.ConsumeErrorRequest());

    engine.Deactivate();
}

TEST_F(Layer3Test, DISABLED_ConvertKeyRejectsYomiLongerThanFiveChars)
{
    ChmEngine engine;
    engine.Activate();

    InputKeys(engine, "ATATAMERUA", L"あたためるあ");

    bool endComposition = true;
    ChmKeyEvent selectKey = MakeKey(VK_SPACE, engine.GetState());

    EXPECT_EQ(ChmFuncType::CompSelect, selectKey.GetType());
    EXPECT_TRUE(engine.UpdateComposition(selectKey, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_TRUE(engine.ConsumeErrorRequest());
    EXPECT_EQ(ChmEngine::State::Inputing, engine.GetState());
    EXPECT_TRUE(engine.HasComposition());
    EXPECT_EQ(L"あたためるあ", engine.GetCompositionStr());

    engine.Deactivate();
}

TEST_F(Layer3Test, InvalidKeysDuringSelectionAreEatenAndKeepSelectingState)
{
    const WPARAM keys[] = { VK_RETURN, VK_LEFT };

    for (WPARAM key : keys) {
        SCOPED_TRACE(key);

        ChmEngine engine;
        engine.Activate();
        engine.SetIME(TRUE);

        InputKeys(engine, "KOU", L"こう");

        ChmCandidatePage page;
        StartSelection(engine, page);

        EXPECT_TRUE(engine.IsKeyEaten(key));

        ChmKeyEvent invalidKey = MakeKey(key, engine.GetState());
        bool endComposition = true;
        EXPECT_TRUE(engine.UpdateComposition(invalidKey, endComposition));
        EXPECT_FALSE(endComposition);
        EXPECT_TRUE(engine.ConsumeErrorRequest());
        EXPECT_EQ(ChmEngine::State::Selecting, engine.GetState());
        EXPECT_TRUE(engine.HasComposition());
        EXPECT_EQ(L"こう", engine.GetCompositionStr());
        EXPECT_TRUE(engine.GetCandidatePage(page));

        engine.Deactivate();
    }
}
