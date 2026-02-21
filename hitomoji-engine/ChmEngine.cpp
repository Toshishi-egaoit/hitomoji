// ChmEngine.cpp 
// Copyright (C) 2026 Hitomoji Project. All rights reserved.
#include <cctype>
#include <map>
#include "ChmRawInputStore.h"
#include "ChmEngine.h"
#include "ChmRomajiConverter.h"
#include "ChmConfig.h"
#include "ChmKeyEvent.h"
#include "Hitomoji.h"

ChmConfig* ChmEngine::_pConfig = nullptr;

ChmEngine::ChmEngine() 
	: _isON(FALSE), _hasComposition(FALSE),_converted(L""), _pending(L"") {
	_pRawInputStore = new ChmRawInputStore();
}

ChmEngine::~ChmEngine() {
	if (_pRawInputStore) delete _pRawInputStore;
}

void ChmEngine::InitConfig() {
	// ChmConfigの初期化
	ChmConfig* newConfig = new ChmConfig();
	BOOL bSuccess = newConfig->LoadFile();
	if (!bSuccess && _pConfig) {
		delete newConfig;
		OutputDebugString(L"   > keeping old _pConfig");
	} else {
		if (!bSuccess) {
			newConfig->InitConfig();
			OutputDebugString(L"   > using empty config");
		}
		delete _pConfig;
		_pConfig = newConfig;
		ChmLogger::Info((std::wstring(L"=== Configs ===\n") + _pConfig->Dump()).c_str());
		if (_pConfig->HasErrors())
		{
			ChmLogger::Warn((std::wstring(L"===ERRORS===\n") + _pConfig->DumpErrors()).c_str());
		}
	}

	return ;
}

BOOL ChmEngine::IsKeyEaten(WPARAM wp) {
    // IMEがOFFなら全てfalse
    if (!_isON) return FALSE;

    ChmKeyEvent ev(wp, 0);

	// 文字入力なら、常にIMEが食う
	if (ev.GetType() == ChmKeyEvent::Type::CharInput) return TRUE;

#ifdef _DEBUG
	// バージョン情報キーもIMEが食う
	if (ev.GetType() == ChmKeyEvent::Type::VersionInfo) return TRUE;
#endif

	// Compositonが存在する状態の特殊キーはIMEが食う
	if (_hasComposition && ev.GetType() != ChmKeyEvent::Type::None ) return TRUE;

	// それ以外は食わない
    return FALSE;
}

void ChmEngine::UpdateComposition(const ChmKeyEvent& keyEvent, bool& pEndComposition){
	ChmKeyEvent::Type _type = keyEvent.GetType();

	// 確定キー
	switch (_type) {
#ifdef _DEBUG
      case ChmKeyEvent::Type::VersionInfo:
            _converted = HM_VERSION L"(" __DATE__ L" " __TIME__ L")";
            _pending = L"";
            _hasComposition = TRUE;
            break;
#endif
      case ChmKeyEvent::Type::CommitNonConvert: // 無変換確定
			// 未変換部分も含めて全確定
            _hasComposition = FALSE;
			break ;
        case ChmKeyEvent::Type::CommitKatakana: // カタカナ変換
			ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending, 
				_pConfig->GetBool(L"ui",L"Backspace-unit-symbol"));
            _converted = ChmRomajiConverter::HiraganaToKatakana(_converted);
            _hasComposition = FALSE;
            break ;
        case ChmKeyEvent::Type::CommitKana:     // ひらがな変換
			ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending,
				_pConfig->GetBool(L"ui",L"Backspace-unit-symbol"));
            _hasComposition = FALSE;
            break;
        case ChmKeyEvent::Type::CommitAscii:    // ASCII確定
            _converted = _pRawInputStore->get();
			_pending = L"";
            _hasComposition = FALSE;
            break;
        case ChmKeyEvent::Type::CommitAsciiWide: // 全角ASCII確定
            _converted = AsciiToWide(_pRawInputStore->get());
			_pending = L"";
            _hasComposition = FALSE;
            break;
        case ChmKeyEvent::Type::Cancel:         // ESCキャンセル
			_converted = L"";
			_pending = L"";
			_hasComposition = FALSE;
			break;
         case ChmKeyEvent::Type::CharInput:
            // 通常の文字入力
            if (!_hasComposition) {
                _pRawInputStore->clear();
                _hasComposition = TRUE;
            }
            _pRawInputStore->push(keyEvent.GetChar());
            ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending, 
				_pConfig->GetBool(L"ui",L"backspace-unit-symbol"));
				;
            break;
        case ChmKeyEvent::Type::Backspace:
            {
				if (!_hasComposition) break;
                size_t len = _pRawInputStore->get().size();
                size_t del = ChmRomajiConverter::GetLastRawUnitLength();

                // Backspace の単位設定を考慮（Char / Unit）
				if (!_pConfig->GetBool(L"ui",L"backspace-unit-symbol")) {
                    del = 1;
                }

                // 念のためのガード：過剰な削除要求は全消去
                if (del > len) del = len;

                _pRawInputStore->pop(del);
				OutputDebugStringWithInt(L"[Hitomoji] Backspace %d chars",(ULONG)del);

                // 削除の結果文字がなくなったらCompositionを削除
                if (_pRawInputStore->get().empty()) {
                    _converted = L"";
                    _pending = L"";
                    _hasComposition = FALSE;
                } else {
                    ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending,
						_pConfig->GetBool(L"ui",L"backspace-unit-symbol"));
                }
            }
            break;
        default:
            // その他のキーは何もしない
            break;
	}
	pEndComposition = !_hasComposition;

	return ;
}

void ChmEngine::PostUpdateComposition(){
	OutputDebugString(L"[Hitomoji] PostUpdateComposition");
	// 変換中でなくなった場合は、残りかすを処分
	if (!_hasComposition) {
		ResetStatus();
	}
	return;
}

void ChmEngine::ResetStatus() {
	OutputDebugString(L"[Hitomoji] ResetStatus");
    _hasComposition = FALSE;
    _pRawInputStore->clear();
    _converted = L"";
    _pending = L"";
	return;
}

std::wstring ChmEngine::AsciiToWide(const std::wstring& src)
{
    std::wstring out;
    out.reserve(src.size());

    for (wchar_t c : src) {
        // 英数字・基本記号のみ対象
        if (c >= 0x21 && c <= 0x7E) {
            // 0x21–0x7E は Fullwidth に +0xFEE0
            out.push_back(static_cast<wchar_t>(c + 0xFEE0));
        } else {
            // それ以外はそのまま（保険）
            out.push_back(static_cast<wchar_t>(c));
        }
    }
    return out;
}

std::wstring ChmEngine::GetCompositionStr(){
	if ( _pRawInputStore == nullptr ) return L"";

	// 既に変換済みの文字列を連結して返却
	return _converted + _pending;
}
