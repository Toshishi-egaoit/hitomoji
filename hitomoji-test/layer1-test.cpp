#include "gtest/gtest.h"
#include "ChmEnvironment.h"
#include "ChmKeyEvent.h"
#include "TestConfigHelper.h"
#include "TestKeyEventHelper.h"
#include <string>

namespace {

struct Layer1KeyCase {
    const wchar_t* label;
    WPARAM key;
    ChmEngine::State state;
    bool shift;
    bool ctrl;
    bool alt;
};

class Layer1Test : public ::testing::Test {
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

    TempConfigDir _dir;
    std::wstring _oldBasePath;
};

} // namespace

TEST_F(Layer1Test, RecognizesFunctionKeysInNoneState)
{
    ChmKeyEvent space = MakeKey(VK_SPACE, ChmEngine::State::None);
    EXPECT_EQ(ChmFuncType::CharInputSpace, space.GetType());

    ChmKeyEvent ctrlZ = MakeKey('Z', ChmEngine::State::None,
                                false, true, false);
    EXPECT_EQ(ChmFuncType::UnFinish, ctrlZ.GetType());
    EXPECT_TRUE(ctrlZ.IsUnFinishKey());

#ifdef _DEBUG
    ChmKeyEvent versionInfo = MakeKey('V', ChmEngine::State::None,
                                      true, true, false);
    EXPECT_EQ(ChmFuncType::VersionInfo, versionInfo.GetType());

    ChmKeyEvent reloadIni = MakeKey('R', ChmEngine::State::None,
                                    true, true, false);
    EXPECT_EQ(ChmFuncType::ReloadIni, reloadIni.GetType());
#endif
}

TEST_F(Layer1Test, RecognizesFunctionKeysInInputingState)
{
    EXPECT_EQ(ChmFuncType::CompFinish,
              MakeKey(VK_RETURN).GetType());
    EXPECT_EQ(ChmFuncType::CompFinishHiragana,
              MakeKey(VK_RETURN, ChmEngine::State::Inputing,
                      false, false, true).GetType());
    EXPECT_EQ(ChmFuncType::CompFinishKatakana,
              MakeKey(VK_RETURN, ChmEngine::State::Inputing,
                      true, false, false).GetType());
    EXPECT_EQ(ChmFuncType::CompFinishKey,
              MakeKey(VK_TAB).GetType());
    EXPECT_EQ(ChmFuncType::CompFinishKeyWide,
              MakeKey(VK_TAB, ChmEngine::State::Inputing,
                      true, false, false).GetType());
    EXPECT_EQ(ChmFuncType::CompSelect,
              MakeKey(VK_SPACE).GetType());
    EXPECT_EQ(ChmFuncType::Backspace,
              MakeKey(VK_BACK).GetType());
    EXPECT_EQ(ChmFuncType::Cancel,
              MakeKey(VK_ESCAPE).GetType());
}

TEST_F(Layer1Test, RecognizesFunctionKeysInSelectingState)
{
    EXPECT_EQ(ChmFuncType::SelectNextPage,
              MakeKey(VK_SPACE, ChmEngine::State::Selecting).GetType());
    EXPECT_EQ(ChmFuncType::SelectPrevPage,
              MakeKey(VK_BACK, ChmEngine::State::Selecting).GetType());
    EXPECT_EQ(ChmFuncType::SelectCancel,
              MakeKey(VK_ESCAPE, ChmEngine::State::Selecting).GetType());
    EXPECT_EQ(ChmFuncType::SelectCancelAndInput,
              MakeKey(VK_OEM_1, ChmEngine::State::Selecting,
                      true, false, false).GetType());
    EXPECT_EQ(ChmFuncType::Backspace,
              MakeKey('H', ChmEngine::State::Selecting,
                      false, true, false).GetType());
    EXPECT_EQ(ChmFuncType::CompFinishKey,
              MakeKey('I', ChmEngine::State::Selecting,
                      false, true, false).GetType());
    EXPECT_EQ(ChmFuncType::CompFinishKeyWide,
              MakeKey('I', ChmEngine::State::Selecting,
                      true, true, false).GetType());
    EXPECT_EQ(ChmFuncType::CompFinish,
              MakeKey('M', ChmEngine::State::Selecting,
                      false, true, false).GetType());
}

TEST_F(Layer1Test, RecognizesCharacterInputWhenNoFunctionKeyMatches)
{
    ChmKeyEvent lowerA = MakeKey('A');
    EXPECT_EQ(ChmFuncType::CharInput, lowerA.GetType());
    EXPECT_EQ('a', lowerA.GetChar());

    ChmKeyEvent upperA = MakeKey('A', ChmEngine::State::Inputing,
                                 true, false, false);
    EXPECT_EQ(ChmFuncType::CharInput, upperA.GetType());
    EXPECT_EQ('A', upperA.GetChar());

    ChmKeyEvent capsA = MakeKey('A', ChmEngine::State::Inputing,
                                false, false, false, true);
    EXPECT_EQ(ChmFuncType::CharInput, capsA.GetType());
    EXPECT_EQ('A', capsA.GetChar());

    ChmKeyEvent shiftedDigit = MakeKey('1', ChmEngine::State::Inputing,
                                       true, false, false);
    EXPECT_EQ(ChmFuncType::CharInput, shiftedDigit.GetType());
    EXPECT_EQ('!', shiftedDigit.GetChar());

    ChmKeyEvent selectingChar = MakeKey('A', ChmEngine::State::Selecting);
    EXPECT_EQ(ChmFuncType::SelectInput, selectingChar.GetType());
    EXPECT_EQ('a', selectingChar.GetChar());
}

TEST_F(Layer1Test, IgnoresCtrlOrAltKeysWhenNoFunctionKeyMatches)
{
    EXPECT_EQ(ChmFuncType::None,
              MakeKey('A', ChmEngine::State::Inputing,
                      false, true, false).GetType());
    EXPECT_EQ(ChmFuncType::None,
              MakeKey('A', ChmEngine::State::Inputing,
                      false, false, true).GetType());
    EXPECT_EQ(ChmFuncType::None,
              MakeKey('1', ChmEngine::State::Inputing,
                      false, true, false).GetType());
}


TEST_F(Layer1Test, IgnoresUndefinedFunctionKeyCombinations)
{
    EXPECT_EQ(ChmFuncType::None,
              MakeKey(VK_ESCAPE, ChmEngine::State::Inputing,
                      true, false, false).GetType());
    EXPECT_EQ(ChmFuncType::None,
              MakeKey(VK_BACK, ChmEngine::State::Inputing,
                      true, false, false).GetType());
    EXPECT_EQ(ChmFuncType::None,
              MakeKey(VK_F1, ChmEngine::State::Inputing).GetType());
    EXPECT_EQ(ChmFuncType::None,
              MakeKey(VK_F1, ChmEngine::State::Selecting).GetType());
}


TEST_F(Layer1Test, IgnoresUndefinedSpecialKeyCombinationsByState)
{
    const Layer1KeyCase cases[] = {
        { L"none: enter",      VK_RETURN, ChmEngine::State::None,      false, false, false },
        { L"none: shift+enter",VK_RETURN, ChmEngine::State::None,      true,  false, false },
        { L"none: alt+enter",  VK_RETURN, ChmEngine::State::None,      false, false, true  },
        { L"none: tab",        VK_TAB,    ChmEngine::State::None,      false, false, false },
        { L"none: backspace",  VK_BACK,   ChmEngine::State::None,      false, false, false },
        { L"none: escape",     VK_ESCAPE, ChmEngine::State::None,      false, false, false },
        { L"none: ctrl+space", VK_SPACE,  ChmEngine::State::None,      false, true,  false },
        { L"none: alt+space",  VK_SPACE,  ChmEngine::State::None,      false, false, true  },

        { L"inputing: ctrl+enter", VK_RETURN, ChmEngine::State::Inputing, false, true,  false },
        { L"inputing: ctrl+tab",   VK_TAB,    ChmEngine::State::Inputing, false, true,  false },
        { L"inputing: alt+tab",    VK_TAB,    ChmEngine::State::Inputing, false, false, true  },
        { L"inputing: shift+back", VK_BACK,   ChmEngine::State::Inputing, true,  false, false },
        { L"inputing: ctrl+back",  VK_BACK,   ChmEngine::State::Inputing, false, true,  false },
        { L"inputing: alt+back",   VK_BACK,   ChmEngine::State::Inputing, false, false, true  },
        { L"inputing: shift+esc",  VK_ESCAPE, ChmEngine::State::Inputing, true,  false, false },
        { L"inputing: ctrl+esc",   VK_ESCAPE, ChmEngine::State::Inputing, false, true,  false },
        { L"inputing: alt+esc",    VK_ESCAPE, ChmEngine::State::Inputing, false, false, true  },
        { L"inputing: ctrl+space", VK_SPACE,  ChmEngine::State::Inputing, false, true,  false },
        { L"inputing: alt+space",  VK_SPACE,  ChmEngine::State::Inputing, false, false, true  },
        { L"inputing: f1",         VK_F1,     ChmEngine::State::Inputing, false, false, false },
        { L"inputing: shift+f1",   VK_F1,     ChmEngine::State::Inputing, true,  false, false },

        { L"selecting: enter",      VK_RETURN, ChmEngine::State::Selecting, false, false, false },
        { L"selecting: shift+enter",VK_RETURN, ChmEngine::State::Selecting, true,  false, false },
        { L"selecting: tab",        VK_TAB,    ChmEngine::State::Selecting, false, false, false },
        { L"selecting: shift+tab",  VK_TAB,    ChmEngine::State::Selecting, true,  false, false },
        { L"selecting: shift+back", VK_BACK,   ChmEngine::State::Selecting, true,  false, false },
        { L"selecting: ctrl+back",  VK_BACK,   ChmEngine::State::Selecting, false, true,  false },
        { L"selecting: alt+back",   VK_BACK,   ChmEngine::State::Selecting, false, false, true  },
        { L"selecting: shift+esc",  VK_ESCAPE, ChmEngine::State::Selecting, true,  false, false },
        { L"selecting: ctrl+esc",   VK_ESCAPE, ChmEngine::State::Selecting, false, true,  false },
        { L"selecting: alt+esc",    VK_ESCAPE, ChmEngine::State::Selecting, false, false, true  },
        { L"selecting: ctrl+space", VK_SPACE,  ChmEngine::State::Selecting, false, true,  false },
        { L"selecting: alt+space",  VK_SPACE,  ChmEngine::State::Selecting, false, false, true  },
        { L"selecting: f1",         VK_F1,     ChmEngine::State::Selecting, false, false, false },
        { L"selecting: shift+f1",   VK_F1,     ChmEngine::State::Selecting, true,  false, false },
    };

    for (const auto& c : cases)
    {
        EXPECT_EQ(ChmFuncType::None,
                  MakeKey(c.key, c.state, c.shift, c.ctrl, c.alt).GetType())
            << std::wstring(c.label);
    }
}
TEST_F(Layer1Test, DISABLED_ShiftSpaceIsIgnoredWhenNotDefinedAsFunctionKey)
{
    EXPECT_EQ(ChmFuncType::None,
              MakeKey(VK_SPACE, ChmEngine::State::None,
                      true, false, false).GetType());
    EXPECT_EQ(ChmFuncType::None,
              MakeKey(VK_SPACE, ChmEngine::State::Inputing,
                      true, false, false).GetType());
    EXPECT_EQ(ChmFuncType::None,
              MakeKey(VK_SPACE, ChmEngine::State::Selecting,
                      true, false, false).GetType());
}
TEST_F(Layer1Test, RecognizesUnFinishKeyEvenDuringComposition)
{
    ChmKeyEvent ctrlZ = MakeKey('Z', ChmEngine::State::Inputing,
                                false, true, false);

    EXPECT_EQ(ChmFuncType::None, ctrlZ.GetType());
    EXPECT_TRUE(ctrlZ.IsUnFinishKey());
}

TEST_F(Layer1Test, NavigationKeysFinishCompositionWithoutFunctionKeyDefinition)
{
    ChmKeyEvent::ClearFunctionKey();

    ChmKeyEvent left = MakeKey(VK_LEFT);
    EXPECT_EQ(ChmFuncType::CompFinish, left.GetType());
    EXPECT_TRUE(left.IsNavigationFinish());

    ChmKeyEvent pageDown = MakeKey(VK_NEXT);
    EXPECT_EQ(ChmFuncType::CompFinish, pageDown.GetType());
    EXPECT_TRUE(pageDown.IsNavigationFinish());

    ChmKeyEvent enter = MakeKey(VK_RETURN);
    EXPECT_EQ(ChmFuncType::None, enter.GetType());
    EXPECT_FALSE(enter.IsNavigationFinish());
}

TEST_F(Layer1Test, IsKeyEatenIgnoresEditingKeysWhenThereIsNoComposition)
{
    ChmEngine engine;
    engine.SetIME(TRUE);

    EXPECT_EQ(ChmEngine::State::None, engine.GetState());
    EXPECT_FALSE(engine.HasComposition());

    EXPECT_FALSE(engine.IsKeyEaten(VK_BACK));
    EXPECT_FALSE(engine.IsKeyEaten(VK_ESCAPE));
    EXPECT_FALSE(engine.IsKeyEaten(VK_RETURN));
    EXPECT_FALSE(engine.IsKeyEaten(VK_TAB));
}

TEST_F(Layer1Test, IsKeyEatenReturnsFalseWhenImeIsOff)
{
    ChmEngine engine;
    engine.SetIME(FALSE);

    EXPECT_FALSE(engine.IsKeyEaten('A'));
    EXPECT_FALSE(engine.IsKeyEaten(VK_SPACE));
    EXPECT_FALSE(engine.IsKeyEaten(VK_BACK));
    EXPECT_FALSE(engine.IsKeyEaten(VK_RETURN));
}

TEST_F(Layer1Test, IsKeyEatenIgnoresModifierKeysOnly)
{
    ChmEngine engine;
    engine.SetIME(TRUE);

    EXPECT_FALSE(engine.IsKeyEaten(VK_SHIFT));
    EXPECT_FALSE(engine.IsKeyEaten(VK_CONTROL));
    EXPECT_FALSE(engine.IsKeyEaten(VK_MENU));
    EXPECT_FALSE(engine.IsKeyEaten(VK_CAPITAL));
}

TEST_F(Layer1Test, IsKeyEatenEatsInputAndCompositionKeysWhileInputing)
{
    ChmEngine engine;
    engine.Activate();
    engine.SetIME(TRUE);

    bool endComposition = true;
    ChmKeyEvent key = MakeKey('A', engine.GetState());
    ASSERT_TRUE(engine.UpdateComposition(key, endComposition));
    ASSERT_EQ(ChmEngine::State::Inputing, engine.GetState());
    ASSERT_TRUE(engine.HasComposition());

    EXPECT_TRUE(engine.IsKeyEaten('A'));
    EXPECT_TRUE(engine.IsKeyEaten(VK_BACK));
    EXPECT_TRUE(engine.IsKeyEaten(VK_ESCAPE));
    EXPECT_TRUE(engine.IsKeyEaten(VK_RETURN));
    EXPECT_TRUE(engine.IsKeyEaten(VK_TAB));

    engine.Deactivate();
}
