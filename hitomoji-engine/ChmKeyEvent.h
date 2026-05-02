#pragma once
// Copyright (C) 2026 by Hitomoji Project. All rights reserved.
#include <windows.h>
#include <cstdint>
#include <string>
#include "utils.h"
#include "hitomoji.h"
#include "ChmConfig.h"
#include "ChmEngine.h"

class ChmRawInputStore;

#define VK_HITOMOJI 0 // 仮想キーコード: マウスイベントなどの特殊用途

class ChmKeyEvent {
public:
	typedef SHORT (__stdcall *KeyStateProvider)(int);

    struct FuncKeyDef {
        WPARAM wp;
        bool   needShift;
        bool   needCtrl;
        bool   needAlt;
        ChmFuncType   type;
    };

    struct CharKeyDef {
        WPARAM wp;
        bool   needShift;
        unsigned char   ch;               // CharInput 用
    };

    // --- utility ---
	const std::wstring toString() const { 
		wchar_t buff[80];
		unsigned char ch = GetChar();
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
    ChmKeyEvent(WPARAM wp, LPARAM lp,  ChmEngine::State state = ChmEngine::State::Inputing);
	ChmKeyEvent(ChmKeyEvent::ChmFuncType type); // マウスクリックなどの特殊用途（OnEndEditで使用）

    // --- function-key table helpers ---
    static void ClearFunctionKey();
    static void InitFunctionKey();
    static BOOL ParseFunctionKey(const std::wstring& key,
                                 const std::wstring& value,
                                 ChmConfig::ParseResult&  errorMsg);
    static std::wstring Dump();

    // --- KeyState hook (for testing) ---
	static void SetKeyStateProvider(KeyStateProvider pFunc);

    // --- accessors ---
    ChmFuncType GetType() const { return _type; }
	unsigned char GetChar() const ;
    bool IsShift() const { return _shift; }
	bool IsUnFinishKey() const;
	bool IsNavigationFinish() const { return _isNavigationFinish; }
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
	static bool _ResolveActionName(const std::wstring& name, ChmFuncType& outType);
	static bool _ResolveKeyName(const std::wstring& name, UINT& outVk);
    // --- Win32::GetKeyState hook ---
	static KeyStateProvider pFunc_keyStateProvider;

    void _TranslateByTable();

    WPARAM _wp;
    bool   _shift   = false;
    bool   _control = false;
    bool   _alt     = false;
    bool   _caps    = false;
    ChmEngine::State _state = ChmEngine::State::None;

    ChmFuncType _type = ChmFuncType::None;
	bool _isNavigationFinish = false;
};
