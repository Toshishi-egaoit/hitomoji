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
            L"select_keymap=dk fj sl gh ei ru a; wo qp ty c, vm x. z/ bn 38 47 29 10 56\n");
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

    void InputChar(ChmEngine& engine, WPARAM key, const wchar_t* composition)
    {
        bool endComposition = true;
        ChmKeyEvent keyEvent = MakeKey(key, engine.GetState());

        EXPECT_EQ(ChmFuncType::CharInput, keyEvent.GetType());
        EXPECT_TRUE(engine.UpdateComposition(keyEvent, endComposition));
        EXPECT_FALSE(endComposition);
        EXPECT_EQ(ChmEngine::State::Inputing, engine.GetState());
        EXPECT_EQ(composition, engine.GetCompositionStr());
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

    InputChar(engine, 'K', L"k");
    InputChar(engine, 'O', L"こ");
    InputChar(engine, 'U', L"こう");

    bool endComposition = true;
    ChmKeyEvent selectKey = MakeKey(VK_SPACE, engine.GetState());

    EXPECT_EQ(ChmFuncType::CompSelect, selectKey.GetType());
    EXPECT_TRUE(engine.UpdateComposition(selectKey, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_EQ(ChmEngine::State::Selecting, engine.GetState());
    EXPECT_TRUE(engine.HasComposition());
    EXPECT_EQ(L"こう", engine.GetCompositionStr());

    ChmCandidatePage page;
    EXPECT_TRUE(engine.GetCandidatePage(page));
    EXPECT_EQ(0u, page.page);
    EXPECT_EQ(123u, page.totalCount);
    EXPECT_EQ(40u, page.candidateCount);

    engine.Deactivate();
}

TEST_F(Layer3Test, DISABLED_EnterDuringSelectionIsEatenAsInvalidSelectionKey)
{
    ChmEngine engine;
    engine.Activate();
    engine.SetIME(TRUE);

    InputChar(engine, 'K', L"k");
    InputChar(engine, 'O', L"こ");
    InputChar(engine, 'U', L"こう");

    bool endComposition = true;
    ChmKeyEvent selectKey = MakeKey(VK_SPACE, engine.GetState());
    ASSERT_TRUE(engine.UpdateComposition(selectKey, endComposition));
    ASSERT_FALSE(endComposition);
    ASSERT_EQ(ChmEngine::State::Selecting, engine.GetState());

    EXPECT_TRUE(engine.IsKeyEaten(VK_RETURN));

    ChmKeyEvent enterKey = MakeKey(VK_RETURN, engine.GetState());
    endComposition = true;
    EXPECT_TRUE(engine.UpdateLayer3(enterKey, endComposition, nullptr));
    EXPECT_FALSE(endComposition);
    EXPECT_TRUE(engine.ConsumeErrorRequest());
    EXPECT_EQ(ChmEngine::State::Selecting, engine.GetState());
    EXPECT_TRUE(engine.HasComposition());
    EXPECT_EQ(L"こう", engine.GetCompositionStr());

    engine.Deactivate();
}
