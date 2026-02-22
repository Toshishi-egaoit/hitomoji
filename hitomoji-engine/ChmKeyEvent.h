#pragma once
// Copyright (C) 2026 by Hitomoji Project. All rights reserved.
#include <windows.h>
#include <cstdint>
#include <string>
#include "utils.h"
#include "hitomoji.h"

class ChmRawInputStore;
class ChmConfig;

#define VK_HITOMOJI 0 // 仮想キーコード: マウスイベントなどの特殊用途

class ChmKeyEvent {
public:
	// TODO: タイプ名を実体に合うように修正
    enum class Type {
        None = 0,
        CharInput,          // 通常文字入力
        CommitKana,         // 見たまま確定(ENTER)
        CommitKatakana,     // カタカナ確定(Shift+Enter)
        CommitAscii,        // キーどおり確定(TAB)
        CommitAsciiWide,    // キーどおり確定ワイド(Shift+TAB)
        CommitNonConvert,   // 見たまま確定(VN_LEFTなど) TODO:見たまま確定に将来は統合
        Cancel,             // キャンセル(ESC)
        Backspace,          // 後退(BS)
        Uncommit,           // 確定取消（将来）(CTRL+Z)
        PassThrough,        // 定義解除（Config専用）
		VersionInfo,        // バージョン表示
    };

    struct FuncKeyDef {
        WPARAM wp;
        bool   needShift;
        bool   needCtrl;
        bool   needAlt;
        Type   type;
    };

    struct CharKeyDef {
        WPARAM wp;
        bool   needShift;
        char   ch;               // CharInput 用
    };

    // --- utility ---
	const std::wstring toString() const { 
		wchar_t buff[80];
		wchar_t ch = GetChar();
		wsprintf(buff, L" Type:%d ch=%d(%c) %s %s %s",
			_type,
			ch , 
			((ch < 0x20) ? L'-' : ch & 0x7f),
			_shift ?   L"Shift"  : L"",
			_control ? L"Control": L"",
			_alt ?     L"Alt"    : L"");
		return buff ;
	}

    // --- ctor ---
    ChmKeyEvent(WPARAM wp, LPARAM lp);
	ChmKeyEvent(ChmKeyEvent::Type type); // マウスクリックなどの特殊用途（OnEndEditで使用）

    // --- function-key table helpers ---
    static BOOL ParseFunctionKey(const std::wstring& key,
                                 const std::wstring& value,
                                 std::wstring& errorMsg);
	static const FuncKeyDef* GetFunctionKeyDefinition(const std::wstring& key);

    static void ClearFunctionKeyOverride();

    // --- KeyState hook (for testing) ---
	static void SetGetKeyStateFunc(SHORT (__stdcall *func)(int));

    // --- accessors ---
    Type GetType() const { return _type; }
	wchar_t GetChar() const ;
    bool IsShift() const { return _shift; }
	// ナビゲーションキーか？(処理はしないが、確定処理が必要なキーの判定用)
    bool IsNavigationKey() const { 
		switch (_wp) {
			case VK_LEFT:
			case VK_RIGHT:
			case VK_UP:
			case VK_DOWN:
			case VK_HOME:
			case VK_END:
			case VK_PRIOR: // PageUp
			case VK_NEXT:  // PageDown
				return true;
		}
		return false;
	}

private:
    // --- KeyTable helper ---
	static bool _ResolveActionName(const std::wstring& name, ChmKeyEvent::Type& outType);
	static bool _ResolveKeyName(const std::wstring& name, UINT& outVk);
    // --- KeyState hook ---
	static SHORT (__stdcall *s_getKeyStateFunc)(int);

    void _TranslateByTable();

    WPARAM _wp;
    bool   _shift   = false;
    bool   _control = false;
    bool   _alt     = false;
    bool   _caps    = false;

    Type _type = Type::None;
};
