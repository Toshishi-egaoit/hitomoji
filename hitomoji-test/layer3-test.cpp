#include "gtest/gtest.h"
#include "ChmCandidatePage.h"
#include "ChmEngine.h"
#include "ChmEnvironment.h"
#include "ChmKeyEvent.h"
#include "TestConfigHelper.h"
#include "TestKeyEventHelper.h"
#include <string>

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
    EXPECT_EQ(131u, page.totalCount);
    EXPECT_EQ(40u, page.candidateCount);

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
        EXPECT_EQ(131u, page.totalCount);
        EXPECT_EQ(34u, page.candidateCount);
        EXPECT_EQ(0x9999u, page.candidates[8]); // 9: index 33, 香

        SelectCandidate(engine, '9', L"香");

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
        EXPECT_EQ(131u, page.totalCount);
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
        EXPECT_EQ(0x9999u, page.candidates[8]); // 9: index 33, 香

        SelectCandidate(engine, '9', L"香");

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
    EXPECT_EQ(89u, page.totalCount);
    EXPECT_EQ(40u, page.candidateCount);

    SelectNextPage(engine, page);
    EXPECT_EQ(1u, page.page);
    EXPECT_EQ(89u, page.totalCount);
    EXPECT_EQ(40u, page.candidateCount);

    SelectNextPage(engine, page);
    EXPECT_EQ(2u, page.page);
    EXPECT_EQ(89u, page.totalCount);
    EXPECT_EQ(9u, page.candidateCount);
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
    ASSERT_EQ(9u, page.candidateCount);

	SelectNextPage(engine, page);

    EXPECT_EQ(0u, page.page);
	EXPECT_EQ(89u, page.totalCount);
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
