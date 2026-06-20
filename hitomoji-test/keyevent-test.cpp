#include "gtest/gtest.h"
#include "ChmConfig.h"
#include "ChmKeyEvent.h"
#include "TestKeyEventHelper.h"

// =====================================================
// gtest フィクスチャ
// すべてのテスト前に KeyStateProvider を差し替える
// =====================================================

class FunctionKeyTestBase : public ::testing::Test
{
protected:
    void SetUp() override
    {
        TestKeyEventHelper::Install();
    }

    void TearDown() override
    {
        TestKeyEventHelper::Restore();
    }
};

// =====================================================
// 新設計用テスト（v0.2.3以降）
// 方針：
//  - 内部構造体は検査しない
//  - 実際のキー入力結果(Type)のみ確認する
//  - Init / Clear / 後勝ち を中心に検証
// =====================================================

// ---------------------------------------------
// 初期化テスト
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, InitAndClear)
{
    // 初期化
    ChmKeyEvent::InitFunctionKey();

    // デフォルトキーが存在するか（例：Enter）
    ChmKeyEvent ev(VK_RETURN, 0);
    EXPECT_EQ(ChmFuncType::CompFinish, ev.GetType());

    // クリア
    ChmKeyEvent::ClearFunctionKey();
    ChmKeyEvent ev2(VK_RETURN, 0);
    EXPECT_EQ(ChmFuncType::None, ev2.GetType());
}

TEST_F(FunctionKeyTestBase, DefaultFunctionKeysByState)
{
    ChmKeyEvent::InitFunctionKey();

    ChmKeyEvent spaceNone(VK_SPACE, 0, ChmEngine::State::None);
    EXPECT_EQ(ChmFuncType::CharInputSpace, spaceNone.GetType());

    ChmKeyEvent enter(VK_RETURN, 0);
    EXPECT_EQ(ChmFuncType::CompFinish, enter.GetType());

    TestKeyEventHelper::SetState(true, false, false);
    ChmKeyEvent shiftEnter(VK_RETURN, 0);
    EXPECT_EQ(ChmFuncType::CompFinishKatakana, shiftEnter.GetType());

    TestKeyEventHelper::SetState(false, false, true);
    ChmKeyEvent altEnter(VK_RETURN, 0);
    EXPECT_EQ(ChmFuncType::CompFinishHiragana, altEnter.GetType());

    TestKeyEventHelper::SetState(false, false, false);
    ChmKeyEvent tab(VK_TAB, 0);
    EXPECT_EQ(ChmFuncType::CompFinishKey, tab.GetType());

    TestKeyEventHelper::SetState(true, false, false);
    ChmKeyEvent shiftTab(VK_TAB, 0);
    EXPECT_EQ(ChmFuncType::CompFinishKeyWide, shiftTab.GetType());

    TestKeyEventHelper::SetState(false, false, false);
    ChmKeyEvent spaceInputing(VK_SPACE, 0);
    EXPECT_EQ(ChmFuncType::CompSelect, spaceInputing.GetType());

    ChmKeyEvent backspace(VK_BACK, 0);
    EXPECT_EQ(ChmFuncType::Backspace, backspace.GetType());

    ChmKeyEvent escape(VK_ESCAPE, 0);
    EXPECT_EQ(ChmFuncType::Cancel, escape.GetType());

    ChmKeyEvent spaceSelecting(VK_SPACE, 0, ChmEngine::State::Selecting);
    EXPECT_EQ(ChmFuncType::SelectNextPage, spaceSelecting.GetType());

    ChmKeyEvent backspaceSelecting(VK_BACK, 0, ChmEngine::State::Selecting);
    EXPECT_EQ(ChmFuncType::SelectPrevPage, backspaceSelecting.GetType());

    ChmKeyEvent escapeSelecting(VK_ESCAPE, 0, ChmEngine::State::Selecting);
    EXPECT_EQ(ChmFuncType::SelectCancel, escapeSelecting.GetType());

    TestKeyEventHelper::SetState(false, true, false);
    ChmKeyEvent ctrlZ('Z', 0, ChmEngine::State::None);
    EXPECT_EQ(ChmFuncType::UnFinish, ctrlZ.GetType());
    EXPECT_TRUE(ctrlZ.IsUnFinishKey());

    ChmKeyEvent ctrlHSelecting('H', 0, ChmEngine::State::Selecting);
    EXPECT_EQ(ChmFuncType::Backspace, ctrlHSelecting.GetType());

    ChmKeyEvent ctrlISelecting('I', 0, ChmEngine::State::Selecting);
    EXPECT_EQ(ChmFuncType::CompFinishKey, ctrlISelecting.GetType());

    TestKeyEventHelper::SetState(true, true, false);
    ChmKeyEvent shiftCtrlISelecting('I', 0, ChmEngine::State::Selecting);
    EXPECT_EQ(ChmFuncType::CompFinishKeyWide, shiftCtrlISelecting.GetType());

    TestKeyEventHelper::SetState(false, true, false);
    ChmKeyEvent ctrlMSelecting('M', 0, ChmEngine::State::Selecting);
    EXPECT_EQ(ChmFuncType::CompFinish, ctrlMSelecting.GetType());
}

TEST_F(FunctionKeyTestBase, CharacterInputTranslation)
{
    ChmKeyEvent::ClearFunctionKey();

    ChmKeyEvent lowerA('A', 0);
    EXPECT_EQ(ChmFuncType::CharInput, lowerA.GetType());
    EXPECT_EQ('a', lowerA.GetChar());

    TestKeyEventHelper::SetState(true, false, false);
    ChmKeyEvent upperAByShift('A', 0);
    EXPECT_EQ(ChmFuncType::CharInput, upperAByShift.GetType());
    EXPECT_EQ('A', upperAByShift.GetChar());

    TestKeyEventHelper::SetState(false, false, false, true);
    ChmKeyEvent upperAByCaps('A', 0);
    EXPECT_EQ(ChmFuncType::CharInput, upperAByCaps.GetType());
    EXPECT_EQ('A', upperAByCaps.GetChar());

    TestKeyEventHelper::SetState(true, false, false, true);
    ChmKeyEvent lowerAByShiftCaps('A', 0);
    EXPECT_EQ(ChmFuncType::CharInput, lowerAByShiftCaps.GetType());
    EXPECT_EQ('a', lowerAByShiftCaps.GetChar());

    TestKeyEventHelper::SetState(true, false, false);
    ChmKeyEvent shiftedDigit('1', 0);
    EXPECT_EQ(ChmFuncType::CharInput, shiftedDigit.GetType());
    EXPECT_EQ('!', shiftedDigit.GetChar());

    TestKeyEventHelper::SetState(false, true, false);
    ChmKeyEvent ctrlA('A', 0);
    EXPECT_EQ(ChmFuncType::None, ctrlA.GetType());
}

TEST_F(FunctionKeyTestBase, SelectingCharacterInput)
{
    ChmKeyEvent::ClearFunctionKey();

    ChmKeyEvent ev('A', 0, ChmEngine::State::Selecting);
    EXPECT_EQ(ChmFuncType::SelectInput, ev.GetType());
    EXPECT_EQ('a', ev.GetChar());
}

TEST_F(FunctionKeyTestBase, NavigationKeyFinishesComposition)
{
    ChmKeyEvent::ClearFunctionKey();

    ChmKeyEvent left(VK_LEFT, 0);
    EXPECT_EQ(ChmFuncType::CompFinish, left.GetType());
    EXPECT_TRUE(left.IsNavigationFinish());

    ChmKeyEvent pageDown(VK_NEXT, 0);
    EXPECT_EQ(ChmFuncType::CompFinish, pageDown.GetType());
    EXPECT_TRUE(pageDown.IsNavigationFinish());

    ChmKeyEvent enter(VK_RETURN, 0);
    EXPECT_FALSE(enter.IsNavigationFinish());
}

// ---------------------------------------------
// Parse 正常系
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, ParseAndApply)
{
    ChmKeyEvent::InitFunctionKey();

    ChmConfig::ParseResult error;

    EXPECT_TRUE(
        ChmKeyEvent::ParseFunctionKey(
            L"CTRL+Z",
            L"finish-raw-wide",
            error));

    EXPECT_TRUE(error.level == ChmConfig::ParseLevel::None);

                  // CTRLのみON
    TestKeyEventHelper::SetState(false, true, false);

    // 実際にキーイベントを生成して確認
    ChmKeyEvent ev('Z', 0);
    EXPECT_EQ(ChmFuncType::CompFinishKeyWide, ev.GetType());
}

// ---------------------------------------------
// 各特殊キー名のチェック
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, SpecialKeyNames)
{
    ChmKeyEvent::ClearFunctionKey();
    ChmConfig::ParseResult error;

    // ENTER
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"RETURN", L"finish", error));
    EXPECT_TRUE(error.level == ChmConfig::ParseLevel::None);

    ChmKeyEvent ev1(VK_RETURN, 0);
    EXPECT_EQ(ChmFuncType::CompFinish, ev1.GetType());

    // ESC
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"ESC", L"cancel", error));
    EXPECT_TRUE(error.level == ChmConfig::ParseLevel::None);

    ChmKeyEvent ev2(VK_ESCAPE, 0);
    EXPECT_EQ(ChmFuncType::Cancel, ev2.GetType());
}

// ---------------------------------------------
// SHIFT + 特殊キーの確認
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, ShiftWithSpecialKey)
{
    ChmKeyEvent::ClearFunctionKey();
    ChmConfig::ParseResult error;

    // SHIFT+ENTER を定義
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(
        L"SHIFT+ENTER",
        L"finish-raw",
        error));

    EXPECT_TRUE(error.level == ChmConfig::ParseLevel::None);

    // SHIFTのみON
    TestKeyEventHelper::SetState(true, false, false);

    ChmKeyEvent ev(VK_RETURN, 0);
    EXPECT_EQ(ChmFuncType::CompFinishKey, ev.GetType());
}

// ---------------------------------------------
// 1) 単独アルファベット・数字はエラー
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, SingleAlphaOrDigitIsError)
{
    ChmConfig::ParseResult error;
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"A", L"finish", error));
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"1", L"finish", error));
}

// ---------------------------------------------
// 2) SHIFT+アルファベット / 数字はエラー
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, ShiftWithAlphaOrDigitIsError)
{
    ChmConfig::ParseResult error;
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"SHIFT+A", L"finish", error));
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"SHIFT+1", L"finish", error));
}

// ---------------------------------------------
// 3) CTRL 組み合わせ（特殊/英字/数字）
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, CtrlCombinations)
{
    ChmConfig::ParseResult error;
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"CTRL+ENTER", L"finish", error));
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"CTRL+Z", L"finish-raw", error));
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"CTRL+1", L"finish-raw", error));
}

// ---------------------------------------------
// 4) ALT 組み合わせ
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, AltCombinations)
{
    ChmConfig::ParseResult error;
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"ALT+ENTER", L"finish", error));
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"ALT+Z", L"finish-raw", error));
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"ALT+1", L"finish-raw", error));
}

// ---------------------------------------------
// 5) 複合修飾キー
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, MultiModifierCombinations)
{
    ChmConfig::ParseResult error;
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"SHIFT+CTRL+ENTER", L"finish", error));
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"SHIFT+ALT+ENTER", L"finish", error));
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"CTRL+ALT+ENTER", L"finish", error));
}

// ---------------------------------------------
// 6) 空白耐性
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, WhitespaceTolerance)
{
    ChmConfig::ParseResult error;
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"  CTRL + ENTER  ", L"finish", error));
}

// ---------------------------------------------
// 7) 大文字小文字・記号混在
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, CaseAndSymbolTolerance)
{
    ChmConfig::ParseResult error;
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"ctrl+enter", L"FINISH", error));
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"ctrl+Escape", L"cancel_fiNISH", error));
}

// ---------------------------------------------
// 8) 不正入力
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, InvalidTokens)
{
    ChmConfig::ParseResult error;
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"SHFT+ENTER", L"finish", error));
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"CTRL+@", L"finish", error));
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"CTRL+ENTER", L"unknown", error));
}

// ---------------------------------------------
// 9) 連続++許容 / キーなしエラー
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, DoublePlusAndMissingKey)
{
    ChmConfig::ParseResult error;
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"CTRL++ENTER", L"finish", error));
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"CTRL+", L"finish", error));
}

// ---------------------------------------------
// 追加: 特殊キー名の網羅チェック
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, AllSpecialKeyTokensCovered)
{
    ChmConfig::ParseResult error;

    const wchar_t* keys[] = {
        L"ENTER", L"RETURN", L"TAB", L"BACKSPACE",
        L"ESC", L"ESCAPE", L"SPACE",
        L"LEFT", L"RIGHT", L"UP", L"DOWN",
        L"HOME", L"END", L"PAGEUP", L"PAGEDOWN",
        L"DEL", L"DELETE", L"INS", L"INSERT",
        L"F1", L"F2", L"F3", L"F4", L"F5", L"F6",
        L"F7", L"F8", L"F9", L"F10", L"F11", L"F12"
    };

    for (auto k : keys)
    {
        EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(k, L"finish", error))
            << "Failed key token: " << std::wstring(k);
    }
}

// ---------------------------------------------
// 追加: 機能名の網羅チェック
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, AllActionNamesCovered)
{
    ChmConfig::ParseResult error;

    const wchar_t* actions[] = {
        L"finish",
        L"finish-hiragana",
        L"finish-katakana",
        L"finish-raw",
        L"finish-raw-wide",
        L"backspace",
        L"cancel",
        L"cancel-finish",
        L"select-start",
        L"next-page",
        L"prev-page",
        L"cancel-select",
        L"select-kanji",
        L"space-char"
#ifdef _DEBUG
        ,L"version-info",
        L"reload-ini"
#endif
    };

    for (auto a : actions)
    {
        EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"CTRL+ENTER", a, error))
            << "Failed action token: " << std::wstring(a);
    }
}

// ---------------------------------------------
// 追加: duplicate 定義は警告のみ（後勝ち）
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, DuplicateDefinitionWarning)
{
    ChmKeyEvent::ClearFunctionKey();
    ChmConfig::ParseResult error;

    // 1回目
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(
        L"CTRL+Z",
        L"finish",
        error));
    EXPECT_TRUE(error.level == ChmConfig::ParseLevel::None);

    // 2回目（duplicate）
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(
        L"CTRL+Z",
        L"cancel",
        error));

    // 警告が入っていること
    EXPECT_TRUE(error.level == ChmConfig::ParseLevel::Info);

    // duplicate という文字列を含むこと
    EXPECT_NE(std::wstring::npos, error.message.find(L"duplicate"));
  
    // 後勝ちで cancel が有効になることを確認
    TestKeyEventHelper::SetState(false, true, false);
    ChmKeyEvent ev('Z', 0);
    EXPECT_EQ(ChmFuncType::Cancel, ev.GetType());
}
