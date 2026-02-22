#include "gtest/gtest.h"
#include "ChmKeyEvent.h"

// --------------------------------------------------
// ParseFunctionKey 単体テスト（static版）
// ChmConfig とは無関係に、
// 純粋に文字列パース結果だけを確認する
// --------------------------------------------------

TEST(ParseFunctionKeyTest, CtrlShiftZ)
{
    ChmKeyEvent::ClearFunctionKeyOverride();

    std::wstring error;

    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(
        L"finish-raw",
        L"CTRL+SHIFT+Z",
        error));

	EXPECT_TRUE(error.empty()) << L"(" << error ;

    const ChmKeyEvent::FuncKeyDef* def =
        ChmKeyEvent::GetFunctionKeyDefinition(L"finish-raw");

    ASSERT_NE(nullptr, def);

    EXPECT_EQ('Z', def->wp);
    EXPECT_TRUE(def->needCtrl);
    EXPECT_TRUE(def->needShift);
    EXPECT_FALSE(def->needAlt);
}

TEST(ParseFunctionKeyTest, DuplicateModifierAllowed)
{
    ChmKeyEvent::ClearFunctionKeyOverride();

    std::wstring error;

    EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(
        L"finish-raw",
        L"CTRL+SHIFT+SHIFT+Z",
        error));

	EXPECT_TRUE(error.empty()) << L"(" << error ;

    const ChmKeyEvent::FuncKeyDef* def =
        ChmKeyEvent::GetFunctionKeyDefinition(L"finish-raw");

    ASSERT_NE(nullptr, def);

    EXPECT_EQ('Z', def->wp);
    EXPECT_TRUE(def->needCtrl);
    EXPECT_TRUE(def->needShift);
}

TEST(ParseFunctionKeyTest, MissingKeyNameError)
{
    ChmKeyEvent::ClearFunctionKeyOverride();

    std::wstring error;

    EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(
        L"finish-raw",
        L"CTRL+SHIFT",
        error));

    EXPECT_FALSE(error.empty());
}

// --------------------------------------------------
// 単一キー（特殊キー）正常系 全網羅テスト
// g_keyNameTable にあるキーが単体で正しく解決されることを確認
// --------------------------------------------------
TEST(ParseFunctionKeyTest, SingleSpecialKey_All)
{
    ChmKeyEvent::ClearFunctionKeyOverride();

    struct Case {
        const wchar_t* value;
        UINT expectedVk;
    };

    Case cases[] = {
        { L"enter", VK_RETURN },
        { L"return", VK_RETURN },
        { L"tab", VK_TAB },
        { L"backspace", VK_BACK },
        { L"esc", VK_ESCAPE },
        { L"escape", VK_ESCAPE },
        { L"space", VK_SPACE },
        { L"left", VK_LEFT },
        { L"right", VK_RIGHT },
        { L"up", VK_UP },
        { L"down", VK_DOWN },
        { L"home", VK_HOME },
        { L"end", VK_END },
        { L"pageup", VK_PRIOR },
        { L"pagedown", VK_NEXT },
        { L"del", VK_DELETE },
        { L"delete", VK_DELETE },
        { L"ins", VK_INSERT },
        { L"insert", VK_INSERT },
        { L"f1", VK_F1 }, { L"f2", VK_F2 }, { L"f3", VK_F3 },
        { L"f4", VK_F4 }, { L"f5", VK_F5 }, { L"f6", VK_F6 },
        { L"f7", VK_F7 }, { L"f8", VK_F8 }, { L"f9", VK_F9 },
        { L"f10", VK_F10 }, { L"f11", VK_F11 }, { L"f12", VK_F12 },
    };

    for (const auto& c : cases)
    {
        std::wstring error;

        EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(
            L"finish-raw",
            c.value,
            error)) << "Failed at value=" << std::wstring(c.value);

		EXPECT_TRUE(error.empty()) << L"(" << error ;

        const ChmKeyEvent::FuncKeyDef* def =
            ChmKeyEvent::GetFunctionKeyDefinition(L"finish-raw");

        ASSERT_NE(nullptr, def);
        EXPECT_EQ(c.expectedVk, def->wp);
        EXPECT_FALSE(def->needCtrl);
        EXPECT_FALSE(def->needShift);
        EXPECT_FALSE(def->needAlt);

        // 次ケースのためクリア
        ChmKeyEvent::ClearFunctionKeyOverride();
    }
}

// --------------------------------------------------
// 単一キー（通常ASCIIキー）禁止テスト
// 0x20-0x7E 相当のキーは FunctionKey として許可しない
// --------------------------------------------------
TEST(ParseFunctionKeyTest, SingleAsciiKey_Disallowed)
{
    ChmKeyEvent::ClearFunctionKeyOverride();

    struct Case {
        const wchar_t* value;
    };

    Case cases[] = {
        { L"A" },
        { L"Z" },
        { L"9" },
        { L"." },
        { L"\\" },   // バックスラッシュ
    };

    for (const auto& c : cases)
    {
        std::wstring error;

        EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(
            L"finish-raw",
            c.value,
            error)) << "Unexpected success at value=" << std::wstring(c.value);

        EXPECT_FALSE(error.empty());

        const ChmKeyEvent::FuncKeyDef* def =
            ChmKeyEvent::GetFunctionKeyDefinition(L"finish-raw");

        EXPECT_EQ(nullptr, def);

        ChmKeyEvent::ClearFunctionKeyOverride();
    }
}

// --------------------------------------------------
// SHIFT + 特殊キー（正常系）抽出テスト
// ENTER と SPACE を対象とする
// --------------------------------------------------
TEST(ParseFunctionKeyTest, ShiftPlusSpecialKey)
{
    ChmKeyEvent::ClearFunctionKeyOverride();

    struct Case {
        const wchar_t* value;
        UINT expectedVk;
    };

    Case cases[] = {
        { L"SHIFT+ENTER", VK_RETURN },
        { L"SHIFT+SPACE", VK_SPACE },
    };

    for (const auto& c : cases)
    {
        std::wstring error;

        EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(
            L"finish-raw",
            c.value,
            error)) << "Failed at value=" << std::wstring(c.value);

        EXPECT_TRUE(error.empty()) << L"(" << error ;

        const ChmKeyEvent::FuncKeyDef* def =
            ChmKeyEvent::GetFunctionKeyDefinition(L"finish-raw");

        ASSERT_NE(nullptr, def);
        EXPECT_EQ(c.expectedVk, def->wp);
        EXPECT_FALSE(def->needCtrl);
        EXPECT_TRUE(def->needShift);
        EXPECT_FALSE(def->needAlt);

        ChmKeyEvent::ClearFunctionKeyOverride();
    }
}

// --------------------------------------------------
// SHIFT + 通常ASCIIキー（禁止）抽出テスト
// "a" と ">" を対象とする
// --------------------------------------------------
TEST(ParseFunctionKeyTest, ShiftPlusAscii_Disallowed)
{
    ChmKeyEvent::ClearFunctionKeyOverride();

    struct Case {
        const wchar_t* value;
    };

    Case cases[] = {
        { L"SHIFT+A" },
        { L"SHIFT+z" },
        { L"SHIFT+0" },
        { L"SHIFT+>" },
    };

    for (const auto& c : cases)
    {
        std::wstring error;

        EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(
            L"finish-raw",
            c.value,
            error))
            << "Unexpected success at value=" << std::wstring(c.value);

        EXPECT_FALSE(error.empty());

        const ChmKeyEvent::FuncKeyDef* def =
            ChmKeyEvent::GetFunctionKeyDefinition(L"finish-raw");

        EXPECT_EQ(nullptr, def);

        ChmKeyEvent::ClearFunctionKeyOverride();
    }
}

// --------------------------------------------------
// CTRL / ALT + キー（正常系）抽出テスト
// 特殊キーおよび通常ASCIIキーを対象とする
// --------------------------------------------------
TEST(ParseFunctionKeyTest, CtrlAltCombinations)
{
    ChmKeyEvent::ClearFunctionKeyOverride();

    struct Case {
        const wchar_t* value;
        UINT expectedVk;
        bool ctrl;
        bool shift;
        bool alt;
    };

    Case cases[] = {
        // 特殊キー
        { L"ALT+PAGEUP", VK_PRIOR, false, false, true },
        { L"CTRL+ESCAPE", VK_ESCAPE, true, false, false },

        // 通常ASCIIキー（CTRL/ALT付きは許可）
        { L"CTRL+A", 'A', true, false, false },
        { L"CTRL+Z", 'Z', true, false, false },
        { L"CTRL++1", '1', true, false, false }, // 連続する++をゆるす
        { L"ALT+9", '9', false, false, true },
        // 
    };

    for (const auto& c : cases)
    {
        std::wstring error;

        EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(
            L"finish-raw",
            c.value,
            error))
            << "Failed at value=" << std::wstring(c.value);

        EXPECT_TRUE(error.empty()) << L"(" << error ;

        const ChmKeyEvent::FuncKeyDef* def =
            ChmKeyEvent::GetFunctionKeyDefinition(L"finish-raw");

        ASSERT_NE(nullptr, def);
        EXPECT_EQ(c.expectedVk, def->wp);
        EXPECT_EQ(c.ctrl, def->needCtrl);
        EXPECT_EQ(c.shift, def->needShift);
        EXPECT_EQ(c.alt, def->needAlt);

        ChmKeyEvent::ClearFunctionKeyOverride();
    }
}

// --------------------------------------------------
// 複数モディファイア + キー（正常系）抽出テスト
// a) SHIFT+CTRL
// b) SHIFT+ALT
// c) CTRL+ALT
// --------------------------------------------------
TEST(ParseFunctionKeyTest, MultiModifierCombinations)
{
    ChmKeyEvent::ClearFunctionKeyOverride();

    struct Case {
        const wchar_t* value;
        UINT expectedVk;
        bool ctrl;
        bool shift;
        bool alt;
    };

    Case cases[] = {
        // a) SHIFT + CTRL
        { L"SHIFT+CTRL+F1", VK_F1, true, true, false },

        // b) SHIFT + ALT
        { L"SHIFT+ALT+TAB", VK_TAB, false, true, true },

        // c) CTRL + ALT
        { L"CTRL+ALT+DELETE", VK_DELETE, true, false, true },
    };

    for (const auto& c : cases)
    {
        std::wstring error;

        EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(
            L"finish-raw",
            c.value,
            error))
            << "Failed at value=" << std::wstring(c.value);

        EXPECT_TRUE(error.empty()) << L"(" << error ;

        const ChmKeyEvent::FuncKeyDef* def =
            ChmKeyEvent::GetFunctionKeyDefinition(L"finish-raw");

        ASSERT_NE(nullptr, def);
        EXPECT_EQ(c.expectedVk, def->wp);
        EXPECT_EQ(c.ctrl, def->needCtrl);
        EXPECT_EQ(c.shift, def->needShift);
        EXPECT_EQ(c.alt, def->needAlt);

        ChmKeyEvent::ClearFunctionKeyOverride();
    }
}

// --------------------------------------------------
// 左辺アクション名（正常系）網羅テスト
// 許可されている全アクション名が受理されることを確認
// --------------------------------------------------
TEST(ParseFunctionKeyTest, ActionName_AllValid)
{
    ChmKeyEvent::ClearFunctionKeyOverride();

    // 右辺は安全な特殊キーで統一
    const wchar_t* rhs = L"CTRL+F1";

    const wchar_t* actions[] = {
        L"FINISH",
        L"finish-KATAKANA",
        L"Finish-Raw",
        L"finish_raw_wide",
        L"backspace",
        L"cancel",
        L"cancel_finish",
        L"pass-through",
#ifdef _DEBUG
		L"version-info",
#endif
    };

    for (auto action : actions)
    {
        std::wstring error;

        EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(
            action,
            rhs,
            error)) << "Failed at action=" << std::wstring(action);

        EXPECT_TRUE(error.empty()) << L"(" << error ;

        const ChmKeyEvent::FuncKeyDef* def =
            ChmKeyEvent::GetFunctionKeyDefinition(action);

        ASSERT_NE(nullptr, def);
        EXPECT_EQ(VK_F1, def->wp);
        EXPECT_TRUE(def->needCtrl);
        EXPECT_FALSE(def->needShift);
        EXPECT_FALSE(def->needAlt);

        ChmKeyEvent::ClearFunctionKeyOverride();
    }
}

// --------------------------------------------------
// Canonize テスト（左辺＋右辺）
// Trim確認はここでは行わない
// --------------------------------------------------
TEST(ParseFunctionKeyTest, CanonizeCases)
{
    ChmKeyEvent::ClearFunctionKeyOverride();

    struct Case {
        const wchar_t* lhs;
        const wchar_t* rhs;
        UINT expectedVk;
        bool ctrl;
        bool shift;
        bool alt;
    };

    Case cases[] = {
        // a) 全部大文字
        { L"FINISH-RAW", L"ENTER", VK_RETURN, false, false, false },

        // b) 先頭だけ大文字
        { L"Finish-Raw", L"Ctrl+Enter", VK_RETURN, true, false, false },

        // c) 全部小文字 + モディファイア2つ
        { L"finish-raw", L"ctrl+shift+f2", VK_F2, true, true, false },
    };

    for (const auto& c : cases)
    {
        std::wstring error;

        EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(
            c.lhs,
            c.rhs,
            error)) << "Failed at lhs=" << std::wstring(c.rhs) << L" (" << error.c_str() << L")";

        EXPECT_TRUE(error.empty()) << L"(" << error ;

        const ChmKeyEvent::FuncKeyDef* def =
            ChmKeyEvent::GetFunctionKeyDefinition(L"finish-raw");

        ASSERT_NE(nullptr, def);
        EXPECT_EQ(c.expectedVk, def->wp);
        EXPECT_EQ(c.ctrl, def->needCtrl);
        EXPECT_EQ(c.shift, def->needShift);
        EXPECT_EQ(c.alt, def->needAlt);

        ChmKeyEvent::ClearFunctionKeyOverride();
    }
}

// --------------------------------------------------
// Trim テスト
// 左辺両側空白、+前後空白、最終キー前後空白を確認
// --------------------------------------------------
TEST(ParseFunctionKeyTest, TrimHandling)
{
    ChmKeyEvent::ClearFunctionKeyOverride();

    struct Case {
        const wchar_t* lhs;
        const wchar_t* rhs;
        UINT expectedVk;
        bool ctrl;
        bool shift;
        bool alt;
    };

    Case cases[] = {
        // + の前後空白
        { L"finish-raw", L"CTRL + SHIFT + F3", VK_F3, true, true, false },

        // 最終キー前後空白（特殊キー）
        { L"finish-raw", L"CTRL+  ENTER  ", VK_RETURN, true, false, false },

        // 最終キー前後空白（通常ASCIIキー）
        { L"finish-raw", L"CTRL+  Z  ", 'Z', true, false, false },
    };

    for (const auto& c : cases)
    {
        std::wstring error;

        EXPECT_TRUE(ChmKeyEvent::ParseFunctionKey(
            c.lhs,
            c.rhs,
            error)) << "Failed at lhs=" << std::wstring(c.lhs);

        EXPECT_TRUE(error.empty()) << L"(" << error ;

        const ChmKeyEvent::FuncKeyDef* def =
            ChmKeyEvent::GetFunctionKeyDefinition(L"finish-raw");

        ASSERT_NE(nullptr, def);
        EXPECT_EQ(c.expectedVk, def->wp);
        EXPECT_EQ(c.ctrl, def->needCtrl);
        EXPECT_EQ(c.shift, def->needShift);
        EXPECT_EQ(c.alt, def->needAlt);

        ChmKeyEvent::ClearFunctionKeyOverride();
    }
}

// --------------------------------------------------
// シンタックスエラー系まとめテスト
// --------------------------------------------------
TEST(ParseFunctionKeyTest, SyntaxErrors)
{
    ChmKeyEvent::ClearFunctionKeyOverride();

    struct Case {
        const wchar_t* lhs;
        const wchar_t* rhs;
    };

    Case cases[] = {

        // 存在しないアクション名
        { L"unknown-action", L"CTRL+Z" },

        // 存在しないモディファイア
        { L"finish-raw", L"SUPER+Z" },

        // 末尾がモディファイアのみ
        { L"finish-raw", L"CTRL+SHIFT" },
    };

    for (const auto& c : cases)
    {
        std::wstring error;

        EXPECT_FALSE(ChmKeyEvent::ParseFunctionKey(
            c.lhs,
            c.rhs,
            error))
            << "Unexpected success at lhs=" << std::wstring(c.lhs)
            << " rhs=" << std::wstring(c.rhs);

        EXPECT_FALSE(error.empty());

        const ChmKeyEvent::FuncKeyDef* def =
            ChmKeyEvent::GetFunctionKeyDefinition(c.lhs);

        EXPECT_EQ(nullptr, def);

        ChmKeyEvent::ClearFunctionKeyOverride();
    }
}

