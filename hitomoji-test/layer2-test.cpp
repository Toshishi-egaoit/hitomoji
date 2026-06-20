#include "gtest/gtest.h"
#include "ChmEngine.h"
#include "ChmEnvironment.h"
#include "ChmKeyEvent.h"
#include "TestConfigHelper.h"
#include "TestKeyEventHelper.h"
#include <string>

namespace {

class Layer2Test : public ::testing::Test {
protected:
    void SetUp() override
    {
        g_environment.Init();
        _oldBasePath = g_environment.GetBasePath();
        g_environment.SetBasePath(_dir.BaseDir());
        _dir.WriteFile(L"Hitomoji.ini", L"[ui]\n");
        TestKeyEventHelper::Install();
        ChmKeyEvent::InitFunctionKey();
    }

    void TearDown() override
    {
        ChmKeyEvent::InitFunctionKey();
        TestKeyEventHelper::Restore();
        g_environment.SetBasePath(_oldBasePath);
    }

    void WriteMainConfig(const std::wstring& content)
    {
        _dir.WriteFile(L"Hitomoji.ini", content);
    }

    ChmKeyEvent MakeKey(WPARAM key,
                        ChmEngine::State state = ChmEngine::State::Inputing,
                        bool shift = false,
                        bool ctrl = false,
                        bool alt = false,
                        bool caps = false)
    {
        TestKeyEventHelper::SetState(shift, ctrl, alt, caps);
        return ChmKeyEvent(key, 0, state);
    }

    void InputChar(ChmEngine& engine, WPARAM key, const wchar_t* composition,
                   bool shift = false)
    {
        bool endComposition = true;
        ChmKeyEvent keyEvent = MakeKey(key, engine.GetState(), shift);

        EXPECT_EQ(ChmFuncType::CharInput, keyEvent.GetType());
        EXPECT_TRUE(engine.UpdateComposition(keyEvent, endComposition));
        EXPECT_FALSE(endComposition);
        EXPECT_EQ(ChmEngine::State::Inputing, engine.GetState());
        EXPECT_TRUE(engine.HasComposition());
        EXPECT_EQ(composition, engine.GetCompositionStr());
    }

    void FinishComposition(ChmEngine& engine,
                           WPARAM key,
                           ChmFuncType expectedType,
                           const wchar_t* committed,
                           bool shift = false,
                           bool ctrl = false,
                           bool alt = false)
    {
        bool endComposition = false;
        ChmKeyEvent keyEvent = MakeKey(key, engine.GetState(), shift, ctrl, alt);

        EXPECT_EQ(expectedType, keyEvent.GetType());
        EXPECT_TRUE(engine.UpdateComposition(keyEvent, endComposition));
        EXPECT_TRUE(endComposition);
        EXPECT_EQ(ChmEngine::State::None, engine.GetState());
        EXPECT_FALSE(engine.HasComposition());
        EXPECT_EQ(committed, engine.GetCompositionStr());

        engine.PostUpdateComposition();
        EXPECT_EQ(L"", engine.GetCompositionStr());
    }

    void ExpectIgnoredKey(ChmEngine& engine,
                          WPARAM key,
                          bool shift = false,
                          bool ctrl = false,
                          bool alt = false)
    {
        bool endComposition = true;
        ChmKeyEvent keyEvent = MakeKey(key, engine.GetState(), shift, ctrl, alt);

        EXPECT_EQ(ChmFuncType::None, keyEvent.GetType());
        EXPECT_FALSE(engine.UpdateComposition(keyEvent, endComposition));
        EXPECT_FALSE(endComposition);
        EXPECT_EQ(ChmEngine::State::None, engine.GetState());
        EXPECT_FALSE(engine.HasComposition());
        EXPECT_EQ(L"", engine.GetCompositionStr());
    }

    void Backspace(ChmEngine& engine, const wchar_t* composition)
    {
        bool endComposition = true;
        ChmKeyEvent keyEvent = MakeKey(VK_BACK, engine.GetState());

        EXPECT_EQ(ChmFuncType::Backspace, keyEvent.GetType());
        EXPECT_TRUE(engine.UpdateComposition(keyEvent, endComposition));
        EXPECT_EQ(composition, engine.GetCompositionStr());
        if (composition[0] == L'\0')
        {
            EXPECT_TRUE(endComposition);
            EXPECT_EQ(ChmEngine::State::None, engine.GetState());
            EXPECT_FALSE(engine.HasComposition());
        }
        else
        {
            EXPECT_FALSE(endComposition);
            EXPECT_EQ(ChmEngine::State::Inputing, engine.GetState());
            EXPECT_TRUE(engine.HasComposition());
        }
    }

    void CancelComposition(ChmEngine& engine)
    {
        bool endComposition = false;
        ChmKeyEvent keyEvent = MakeKey(VK_ESCAPE, engine.GetState());

        EXPECT_EQ(ChmFuncType::Cancel, keyEvent.GetType());
        EXPECT_TRUE(engine.UpdateComposition(keyEvent, endComposition));
        EXPECT_TRUE(endComposition);
        EXPECT_EQ(ChmEngine::State::None, engine.GetState());
        EXPECT_FALSE(engine.HasComposition());
        EXPECT_EQ(L"", engine.GetCompositionStr());
    }

    void UnFinishComposition(ChmEngine& engine, const wchar_t* composition)
    {
        bool endComposition = true;
        ChmKeyEvent keyEvent = MakeKey('Z', engine.GetState(), false, true);

        EXPECT_EQ(ChmFuncType::UnFinish, keyEvent.GetType());
        EXPECT_TRUE(engine.UpdateComposition(keyEvent, endComposition));
        EXPECT_FALSE(endComposition);
        EXPECT_EQ(ChmEngine::State::Inputing, engine.GetState());
        EXPECT_TRUE(engine.HasComposition());
        EXPECT_TRUE(engine.UseUndoEditSession());
        EXPECT_EQ(composition, engine.GetCompositionStr());
    }

    bool IsCtrlZKeyEaten(ChmEngine& engine)
    {
        TestKeyEventHelper::SetState(false, true);
        bool result = engine.IsKeyEaten('Z') != FALSE;
        TestKeyEventHelper::ResetState();
        return result;
    }

    TempConfigDir _dir;
    std::wstring _oldBasePath;
};

} // namespace

TEST_F(Layer2Test, CharInputStartsComposition)
{
    ChmEngine engine;
    engine.Activate();

    bool endComposition = true;
    ChmKeyEvent key = MakeKey('A', engine.GetState());

    EXPECT_EQ(ChmFuncType::CharInput, key.GetType());
    EXPECT_TRUE(engine.UpdateComposition(key, endComposition));
    EXPECT_FALSE(endComposition);
    EXPECT_EQ(ChmEngine::State::Inputing, engine.GetState());
    EXPECT_TRUE(engine.HasComposition());
    EXPECT_EQ(L"あ", engine.GetCompositionStr());

    engine.Deactivate();
}

TEST_F(Layer2Test, RomajiInputUpdatesCompositionStepByStep)
{
    ChmEngine engine;
    engine.Activate();

    InputChar(engine, 'K', L"k");
    InputChar(engine, 'A', L"か");
    InputChar(engine, 'K', L"かk");
    InputChar(engine, 'Y', L"かky");
    InputChar(engine, 'A', L"かきゃ");
    InputChar(engine, 'K', L"かきゃk");
    InputChar(engine, 'Y', L"かきゃky");
    InputChar(engine, 'B', L"かきゃkyb");
    InputChar(engine, 'K', L"かきゃkybk");
    InputChar(engine, '1', L"かきゃkybk！", true);

    engine.Deactivate();
}

TEST_F(Layer2Test, RomajiInputAcceptsDigitsAndUppercaseLetters)
{
    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, 'K', L"k");
        InputChar(engine, '1', L"k1");

        engine.Deactivate();
    }
    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, 'K', L"K", true);
        InputChar(engine, 'A', L"か");

        engine.Deactivate();
    }
    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, 'K', L"k");
        InputChar(engine, 'A', L"か", true);

        engine.Deactivate();
    }
}

TEST_F(Layer2Test, UserKeyTableOverridesLayer2Conversion)
{
    WriteMainConfig(
        L"[ui]\n"
        L"[key-table]\n"
        L"->=→\n"
        L"/k=々\n"
        L"/kk=〻\n");

    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, VK_OEM_MINUS, L"ー");
        InputChar(engine, VK_OEM_PERIOD, L"→", true);

        engine.Deactivate();
    }
    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, VK_OEM_2, L"/");
        InputChar(engine, 'K', L"々");

        engine.Deactivate();
    }
    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, VK_OEM_2, L"/");
        InputChar(engine, 'K', L"々");
        InputChar(engine, 'K', L"〻");

        engine.Deactivate();
    }
}

TEST_F(Layer2Test, UserKeyTableOverridesDefaultLayer2Conversion)
{
    WriteMainConfig(
        L"[ui]\n"
        L"[key-table]\n"
        L"wi=うぃ\n");

    ChmEngine engine;
    engine.Activate();

    InputChar(engine, 'W', L"w");
    InputChar(engine, 'I', L"うぃ");
    InputChar(engine, 'K', L"うぃk");
    InputChar(engine, 'A', L"うぃか");

    engine.Deactivate();
}

TEST_F(Layer2Test, UserKeyTableUsesLongestMatchForMultiCharacterKeys)
{
    WriteMainConfig(
        L"[ui]\n"
        L"[key-table]\n"
        L"->=→\n"
        L"-->=→→\n");

    ChmEngine engine;
    engine.Activate();

    InputChar(engine, VK_OEM_MINUS, L"ー");
    InputChar(engine, VK_OEM_MINUS, L"ーー");
    InputChar(engine, VK_OEM_PERIOD, L"→→", true);

    engine.Deactivate();
}

TEST_F(Layer2Test, FinishKeysEndCompositionAndKeepCommitTextForTsf)
{
    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, 'K', L"k");
        InputChar(engine, 'A', L"か");
        FinishComposition(engine, VK_RETURN, ChmFuncType::CompFinish, L"か");

        engine.Deactivate();
    }
    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, 'K', L"k");
        InputChar(engine, 'A', L"か");
        FinishComposition(engine, VK_RETURN, ChmFuncType::CompFinishHiragana,
                          L"か", false, false, true);

        engine.Deactivate();
    }
    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, 'K', L"k");
        InputChar(engine, 'A', L"か");
        FinishComposition(engine, VK_RETURN, ChmFuncType::CompFinishKatakana,
                          L"カ", true);

        engine.Deactivate();
    }
    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, 'K', L"k");
        InputChar(engine, 'A', L"か");
        FinishComposition(engine, VK_TAB, ChmFuncType::CompFinishKey, L"ka");

        engine.Deactivate();
    }
    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, 'K', L"k");
        InputChar(engine, 'A', L"か");
        FinishComposition(engine, VK_TAB, ChmFuncType::CompFinishKeyWide,
                          L"ｋａ", true);

        engine.Deactivate();
    }
}

TEST_F(Layer2Test, KatakanaFinishCommitsConvertedAndPendingText)
{
    ChmEngine engine;
    engine.Activate();

    InputChar(engine, 'K', L"k");
    InputChar(engine, 'A', L"か");
    InputChar(engine, 'K', L"かk");
    InputChar(engine, 'Y', L"かky");
    FinishComposition(engine, VK_RETURN, ChmFuncType::CompFinishKatakana,
                      L"カky", true);

    engine.Deactivate();
}

TEST_F(Layer2Test, FinishKeysAreIgnoredWhenThereIsNoComposition)
{
    ChmEngine engine;
    engine.Activate();

    EXPECT_EQ(ChmEngine::State::None, engine.GetState());
    EXPECT_FALSE(engine.HasComposition());

    ExpectIgnoredKey(engine, VK_RETURN);
    ExpectIgnoredKey(engine, VK_RETURN, false, false, true);
    ExpectIgnoredKey(engine, VK_RETURN, true);
    ExpectIgnoredKey(engine, VK_TAB);
    ExpectIgnoredKey(engine, VK_TAB, true);

    engine.Deactivate();
}

TEST_F(Layer2Test, BackspaceDeletesLastConvertedUnitByDefault)
{
    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, 'K', L"k");
        InputChar(engine, 'Y', L"ky");
        Backspace(engine, L"");

        engine.Deactivate();
    }
    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, 'K', L"k");
        InputChar(engine, 'Y', L"ky");
        InputChar(engine, 'A', L"きゃ");
        Backspace(engine, L"");

        engine.Deactivate();
    }
    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, 'K', L"k");
        InputChar(engine, 'A', L"か");
        InputChar(engine, 'K', L"かk");
        InputChar(engine, 'Y', L"かky");
        Backspace(engine, L"か");

        engine.Deactivate();
    }
}

TEST_F(Layer2Test, BackspaceDeletesUserKeyTableUnitByDefault)
{
    WriteMainConfig(
        L"[ui]\n"
        L"[key-table]\n"
        L"->=→\n");

    ChmEngine engine;
    engine.Activate();

    InputChar(engine, VK_OEM_MINUS, L"ー");
    InputChar(engine, VK_OEM_PERIOD, L"→", true);
    Backspace(engine, L"");

    engine.Deactivate();
}

TEST_F(Layer2Test, BackspaceDeletesOneRawKeyWhenConfigured)
{
    WriteMainConfig(
        L"[ui]\n"
        L"backspace-unit-symbol=false\n"
        L"[key-table]\n"
        L"->=→\n");

    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, 'K', L"k");
        InputChar(engine, 'Y', L"ky");
        InputChar(engine, 'A', L"きゃ");
        Backspace(engine, L"ky");
        Backspace(engine, L"k");
        Backspace(engine, L"");

        engine.Deactivate();
    }
    {
        ChmEngine engine;
        engine.Activate();

        InputChar(engine, VK_OEM_MINUS, L"ー");
        InputChar(engine, VK_OEM_PERIOD, L"→", true);
        Backspace(engine, L"ー");
        Backspace(engine, L"");

        engine.Deactivate();
    }
}

TEST_F(Layer2Test, BackspaceIsIgnoredWhenThereIsNoComposition)
{
    ChmEngine engine;
    engine.Activate();

    EXPECT_EQ(ChmEngine::State::None, engine.GetState());
    EXPECT_FALSE(engine.HasComposition());

    ExpectIgnoredKey(engine, VK_BACK);

    engine.Deactivate();
}

TEST_F(Layer2Test, EscapeClearsConvertedComposition)
{
    ChmEngine engine;
    engine.Activate();

    InputChar(engine, 'K', L"k");
    InputChar(engine, 'A', L"か");
    CancelComposition(engine);

    engine.Deactivate();
}

TEST_F(Layer2Test, EscapeClearsPendingComposition)
{
    ChmEngine engine;
    engine.Activate();

    InputChar(engine, 'K', L"k");
    InputChar(engine, 'Y', L"ky");
    CancelComposition(engine);

    engine.Deactivate();
}

TEST_F(Layer2Test, EscapeClearsConvertedAndPendingComposition)
{
    ChmEngine engine;
    engine.Activate();

    InputChar(engine, 'K', L"k");
    InputChar(engine, 'A', L"か");
    InputChar(engine, 'K', L"かk");
    InputChar(engine, 'Y', L"かky");
    CancelComposition(engine);

    engine.Deactivate();
}

TEST_F(Layer2Test, EscapeClearsUserKeyTableComposition)
{
    WriteMainConfig(
        L"[ui]\n"
        L"[key-table]\n"
        L"->=→\n");

    ChmEngine engine;
    engine.Activate();

    InputChar(engine, VK_OEM_MINUS, L"ー");
    InputChar(engine, VK_OEM_PERIOD, L"→", true);
    CancelComposition(engine);

    engine.Deactivate();
}

TEST_F(Layer2Test, EscapeIsIgnoredWhenThereIsNoComposition)
{
    ChmEngine engine;
    engine.Activate();

    EXPECT_EQ(ChmEngine::State::None, engine.GetState());
    EXPECT_FALSE(engine.HasComposition());

    ExpectIgnoredKey(engine, VK_ESCAPE);

    engine.Deactivate();
}

TEST_F(Layer2Test, CtrlZRestoresCompositionAfterExplicitFinish)
{
    ChmEngine engine;
    engine.Activate();
    engine.SetIME(TRUE);

    InputChar(engine, 'K', L"k");
    InputChar(engine, 'A', L"か");
    FinishComposition(engine, VK_RETURN, ChmFuncType::CompFinish, L"か");

    EXPECT_TRUE(engine.CanUnFinish());
    EXPECT_EQ(1, engine.GetUndoDeleteLength());
    EXPECT_TRUE(IsCtrlZKeyEaten(engine));

    UnFinishComposition(engine, L"か");

    engine.Deactivate();
}

TEST_F(Layer2Test, CtrlZRestoresRawCompositionAfterRawFinish)
{
    ChmEngine engine;
    engine.Activate();
    engine.SetIME(TRUE);

    InputChar(engine, 'K', L"k");
    InputChar(engine, 'A', L"か");
    FinishComposition(engine, VK_TAB, ChmFuncType::CompFinishKey, L"ka");

    EXPECT_TRUE(engine.CanUnFinish());
    EXPECT_EQ(2, engine.GetUndoDeleteLength());
    EXPECT_TRUE(IsCtrlZKeyEaten(engine));

    UnFinishComposition(engine, L"か");

    engine.Deactivate();
}

TEST_F(Layer2Test, CtrlZIsNotAvailableAfterEscapeCancel)
{
    ChmEngine engine;
    engine.Activate();
    engine.SetIME(TRUE);

    InputChar(engine, 'K', L"k");
    InputChar(engine, 'A', L"か");
    CancelComposition(engine);

    EXPECT_FALSE(engine.CanUnFinish());
    EXPECT_FALSE(IsCtrlZKeyEaten(engine));

    engine.Deactivate();
}

TEST_F(Layer2Test, CtrlZIsNotAvailableAfterNavigationFinish)
{
    ChmEngine engine;
    engine.Activate();
    engine.SetIME(TRUE);

    InputChar(engine, 'K', L"k");
    InputChar(engine, 'A', L"か");
    FinishComposition(engine, VK_LEFT, ChmFuncType::CompFinish, L"か");

    EXPECT_FALSE(engine.CanUnFinish());
    EXPECT_FALSE(IsCtrlZKeyEaten(engine));

    engine.Deactivate();
}
