#include "gtest/gtest.h"
#include "ChmKeyEvent.h"

// =====================================================
// テスト用 KeyStateProvider
// =====================================================

static bool g_shift = false;
static bool g_ctrl  = false;
static bool g_alt   = false;
static bool g_caps  = false;

static SHORT TestKeyStateProvider(int vkey)
{
    switch (vkey)
    {
    case VK_SHIFT:   return g_shift ? 0x8000 : 0;
    case VK_CONTROL: return g_ctrl  ? 0x8000 : 0;
    case VK_MENU:    return g_alt   ? 0x8000 : 0;
    case VK_CAPITAL: return g_caps  ? 0x0001 : 0;
    default:         return 0;
    }
}

static void SetKeyState(bool shift, bool ctrl, bool alt, bool caps = false)
{
    g_shift = shift;
    g_ctrl  = ctrl;
    g_alt   = alt;
    g_caps  = caps;
}

// =====================================================
// gtest フィクスチャ
// すべてのテスト前に KeyStateProvider を差し替える
// =====================================================

class FunctionKeyTestBase : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ChmKeyEvent::SetKeyStateProvider(TestKeyStateProvider);
        SetKeyState(false, false, false, false);
    }

    void TearDown() override
    {
        ChmKeyEvent::SetKeyStateProvider(::GetKeyState);
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
    EXPECT_EQ(ChmKeyEvent::Type::CommitKana, ev.GetType());

    // クリア
    ChmKeyEvent::ClearFunctionKey();
    ChmKeyEvent ev2(VK_RETURN, 0);
    EXPECT_EQ(ChmKeyEvent::Type::None, ev2.GetType());
}

// ---------------------------------------------
// Parse 正常系
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, ParseAndApply)
{
    ChmKeyEvent::InitFunctionKey();

    std::wstring error;

    EXPECT_TRUE(
        ChmKeyEvent::ParseFunctionKey(
            L"CTRL+Z",
            L"finish-raw-wide",
            error));

    EXPECT_TRUE(error.empty());

                  // CTRLのみON
    SetKeyState(false, true, false);

    // 実際にキーイベントを生成して確認
    ChmKeyEvent ev('Z', 0);
    EXPECT_EQ(ChmKeyEvent::Type::CommitAsciiWide, ev.GetType());
}

// ---------------------------------------------
// 各特殊キー名のチェック
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, SpecialKeyNames)
{
    ChmKeyEvent::ClearFunctionKey();
    std::wstring error;

    // ENTER
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"RETURN", L"finish", error));
    EXPECT_TRUE(error.empty());

    ChmKeyEvent ev1(VK_RETURN, 0);
    EXPECT_EQ(ChmKeyEvent::Type::CommitKana, ev1.GetType());

    // ESC
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"ESC", L"cancel", error));
    EXPECT_TRUE(error.empty());

    ChmKeyEvent ev2(VK_ESCAPE, 0);
    EXPECT_EQ(ChmKeyEvent::Type::Cancel, ev2.GetType());
}

// ---------------------------------------------
// SHIFT + 特殊キーの確認
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, ShiftWithSpecialKey)
{
    ChmKeyEvent::ClearFunctionKey();
    std::wstring error;

    // SHIFT+ENTER を定義
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(
        L"SHIFT+ENTER",
        L"finish-raw",
        error));

    EXPECT_TRUE(error.empty());

    // SHIFTのみON
    SetKeyState(true, false, false);

    ChmKeyEvent ev(VK_RETURN, 0);
    EXPECT_EQ(ChmKeyEvent::Type::CommitAscii, ev.GetType());
}

// ---------------------------------------------
// 1) 単独アルファベット・数字はエラー
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, SingleAlphaOrDigitIsError)
{
    std::wstring error;
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"A", L"finish", error));
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"1", L"finish", error));
}

// ---------------------------------------------
// 2) SHIFT+アルファベット / 数字はエラー
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, ShiftWithAlphaOrDigitIsError)
{
    std::wstring error;
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"SHIFT+A", L"finish", error));
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"SHIFT+1", L"finish", error));
}

// ---------------------------------------------
// 3) CTRL 組み合わせ（特殊/英字/数字）
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, CtrlCombinations)
{
    std::wstring error;
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"CTRL+ENTER", L"finish", error));
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"CTRL+Z", L"finish-raw", error));
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"CTRL+1", L"finish-raw", error));
}

// ---------------------------------------------
// 4) ALT 組み合わせ
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, AltCombinations)
{
    std::wstring error;
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"ALT+ENTER", L"finish", error));
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"ALT+Z", L"finish-raw", error));
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"ALT+1", L"finish-raw", error));
}

// ---------------------------------------------
// 5) 複合修飾キー
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, MultiModifierCombinations)
{
    std::wstring error;
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"SHIFT+CTRL+ENTER", L"finish", error));
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"SHIFT+ALT+ENTER", L"finish", error));
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"CTRL+ALT+ENTER", L"finish", error));
}

// ---------------------------------------------
// 6) 空白耐性
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, WhitespaceTolerance)
{
    std::wstring error;
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"  CTRL + ENTER  ", L"finish", error));
}

// ---------------------------------------------
// 7) 大文字小文字・記号混在
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, CaseAndSymbolTolerance)
{
    std::wstring error;
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"ctrl+enter", L"FINISH", error));
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"ctrl+Escape", L"cancel_fiNISH", error));
}

// ---------------------------------------------
// 8) 不正入力
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, InvalidTokens)
{
    std::wstring error;
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"SHFT+ENTER", L"finish", error));
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"CTRL+@", L"finish", error));
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"CTRL+ENTER", L"unknown", error));
}

// ---------------------------------------------
// 9) 連続++許容 / キーなしエラー
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, DoublePlusAndMissingKey)
{
    std::wstring error;
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(L"CTRL++ENTER", L"finish", error));
    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(L"CTRL+", L"finish", error));
}

// ---------------------------------------------
// 追加: 特殊キー名の網羅チェック
// ---------------------------------------------
TEST_F(FunctionKeyTestBase, AllSpecialKeyTokensCovered)
{
    std::wstring error;

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
    std::wstring error;

    const wchar_t* actions[] = {
        L"finish",
        L"finish-katakana",
        L"finish-raw",
        L"finish-raw-wide",
        L"backspace",
        L"cancel",
        L"cancel-finish"
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
    std::wstring error;

    // 1回目
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(
        L"CTRL+Z",
        L"finish",
        error));
    EXPECT_TRUE(error.empty());

    // 2回目（duplicate）
    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(
        L"CTRL+Z",
        L"cancel",
        error));

    // 警告が入っていること
    EXPECT_FALSE(error.empty());

    // duplicate という文字列を含むこと
    EXPECT_NE(std::wstring::npos, error.find(L"duplicate"))
      << "Expected duplicate warning, but got: " << error;
  
    // 後勝ちで cancel が有効になることを確認
    SetKeyState(false, true, false);
    ChmKeyEvent ev('Z', 0);
    EXPECT_EQ(ChmKeyEvent::Type::Cancel, ev.GetType());
}
