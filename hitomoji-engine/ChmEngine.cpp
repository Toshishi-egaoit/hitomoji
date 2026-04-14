// ChmEngine.cpp 
// Copyright (C) 2026 Hitomoji Project. All rights reserved.
#include <cctype>
#include <map>
#include "ChmRawInputStore.h"
#include "ChmEngine.h"
#include "ChmRomajiConverter.h"
#include "ChmKeyEvent.h"
#include "ChmL3Kanji.h"
#include "Hitomoji.h"

ChmEngine::ChmEngine() 
	: _isON(FALSE), _state(State::None), _converted(L""), _pending(L"") 
{
	_pRawInputStore = new ChmRawInputStore();
	_pConfig = nullptr;
	_pL3KanjiDict = nullptr;
	_pL3KanjiSelect = nullptr;
	_pL3Helper = nullptr;
}

ChmEngine::~ChmEngine() {
	if (_pRawInputStore) delete _pRawInputStore;
}

void ChmEngine::Activate() {
	Debug(L"ChmEngine::Activate");
	_initEnv();
	InitConfig();
	_initLayer2();
	_initLayer3();
}

void ChmEngine::Deactivate() {
	ResetStatus();
	delete _pConfig;
	_pConfig = nullptr ;

	delete _pL3KanjiDict;
	_pL3KanjiDict = nullptr ;
	
	delete _pL3Helper;
	_pL3Helper = nullptr ;
}

void ChmEngine::InitConfig() {
	// ChmConfigの初期化
	ChmConfig* newConfig = new ChmConfig();
	BOOL bSuccess = newConfig->LoadFile();

	if (newConfig->HasErrors())
	{
		Warn((std::wstring(L"===Errors===\n") + newConfig->DumpErrors()).c_str());
		Warn((std::wstring(L"===Infos===\n") + newConfig->DumpInfos()).c_str());
	}
	if (!bSuccess && _pConfig) {
		delete newConfig;
		Warn(L"   > keeping old _pConfig");
	} else {
		if (!bSuccess) {
			newConfig->InitConfig();
			Warn(L"   > using empty config");
		}
		delete _pConfig;
		_pConfig = newConfig;
		Info((std::wstring(L"=== Configs ===\n") + _pConfig->Dump()).c_str());
		Info((std::wstring(L"=== Function-Keys ===\n") + ChmKeyEvent::Dump()).c_str());
	}

	// Loggerのログレベルを設定
	{
		std::wstring logLevelStr = ChmConfig::Canonize(_pConfig->GetString(L"UI", L"loglevel"));
		if ( logLevelStr == L"error")     ChmLogger::SetLogLevel(ChmLogger::ErrorLevel);
		else if (logLevelStr == L"warn")  ChmLogger::SetLogLevel(ChmLogger::WarnLevel);
		else if (logLevelStr == L"info")  ChmLogger::SetLogLevel(ChmLogger::InfoLevel);
		else if (logLevelStr == L"debug") ChmLogger::SetLogLevel(ChmLogger::DebugLevel);
		else {
			ChmLogger::SetLogLevel(ChmLogger::InfoLevel);
			Warn(L"   > Invalid LogLevel in config, defaulting to Info");
		}

	}
	return ;
}

std::wstring ChmEngine::GetConfigFile() { 
	return _pConfig->GetConfigFile(); 
}

void ChmEngine::_initEnv() {
	g_environment.Init();
}

void ChmEngine::_initLayer2() {
	// TODO: いまはnullかんすう

}

void ChmEngine::_initLayer3() {
	if (_pL3Helper == nullptr )
		_pL3Helper = new ChmL3Helper ;
	if (_pL3KanjiDict == nullptr )
		_pL3KanjiDict = new ChmL3KanjiDict ;
}

BOOL ChmEngine::IsKeyEaten(WPARAM wp) {
	// IMEがOFFなら全てfalse
	if (!_isON) return FALSE;

	ChmKeyEvent ev(wp, 0, _state);

	// 文字入力なら、常にIMEが食う
	if (ev.GetType() == ChmFuncType::CharInput) return TRUE;
	if (ev.GetType() == ChmFuncType::CharInputSpace) return TRUE;

#ifdef _DEBUG
	// バージョン情報キーもIMEが食う
	if (ev.GetType() == ChmFuncType::VersionInfo) return TRUE;
#endif

	// UNDOに限っては確定直後の入力をIMEが食う
	if (!HasComposition() && ev.GetType() == ChmFuncType::UnFinish) return TRUE;

	// Compositonが存在する状態の特殊キーはIMEが食う
	if (HasComposition() && ev.GetType() != ChmFuncType::None) return TRUE;

	// それ以外は食わない
	return FALSE;
}

void ChmEngine::SetError(void) {
	// TODO:Beep音を出す。（confiファイルでON/OFFできるようにする）
	_pending += L"?";
}

void ChmEngine::UpdateComposition(const ChmKeyEvent& keyEvent, bool& pEndComposition){
	ChmFuncType _type = keyEvent.GetType();

	// 確定キー
	switch (_type) {
		// --------
		// 文字入力
		case ChmFuncType::CharInputSpace: // 1文字目のスペース入力
			// TODO: configで全角空白か半角かを選べるようにする
			_converted = L" ";
			_pending = L"";
			_state = State::None;
			break;
		case ChmFuncType::CharInput:
			// 通常の文字入力
			_state = State::Inputing;
			_pRawInputStore->push(keyEvent.GetChar());
			ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending, 
				_pConfig->GetBool(L"ui",L"backspace-unit-symbol"));
			break;
		case ChmFuncType::Backspace: {
			if (!HasComposition()) break;
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
				_state = State::None;
			} else {
				ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending,
					_pConfig->GetBool(L"ui",L"backspace-unit-symbol"));
			}
			break;
		}
		case ChmFuncType::Cancel:         // ESCキャンセル
			_converted = L"";
			_pending = L"";
			_state = State::None;
			break;
		case ChmFuncType::UnFinish:         // CTRL+Zで未確定状態に戻す
			// TODO:直前の確定文字列を削除する処理が必要
			_pRawInputStore->restore();
			ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending, 
				_pConfig->GetBool(L"ui",L"Backspace-unit-symbol"));
			_state = State::Inputing;
			break;

		// --------
		// 漢字変換に関する処理
		case ChmFuncType::CompSelect:
			// かな漢字変換の選択開始
			_state = State::Selecting;
			_pL3KanjiSelect = new ChmL3KanjiSelect(_pL3KanjiDict, _pL3Helper->GetPageSize());
			if (!_pending.empty() ) {
				// エラーチェック：pendingに文字がある?
				_pending += L"?";
				_state = State::Inputing;
			} else if ( _pL3KanjiSelect->Start(_converted) == FALSE ) {
				// エラーチェック２：_convertedの読みを検索しても見つからない?
				_pending = L"?ym";
				_state = State::Inputing;
			}
			_converted = L"<" + _converted + L">";
			// TODO:(未実装)候補リストの更新
			// TODO:(未実装)選択候補のスレッドへの通知

			break;
		case ChmFuncType::SelectNextPage:
			_converted = _converted + L"+";
			_pL3KanjiSelect->NextPage();
			break;
		case ChmFuncType::SelectPrevPage:
			_converted = _converted + L"-";
			_pL3KanjiSelect->PrevPage();
			break;
		case ChmFuncType::SelectCancel:         // 選択中のキャンセルは入力に戻す
			ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending, 
				_pConfig->GetBool(L"ui",L"Backspace-unit-symbol"));
			_state = State::Inputing;
			break;

		// --------
		// 各種の確定キーの処理
		case ChmFuncType::CompFinishHiragana: // ひらがな変換
			ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending, 
				_pConfig->GetBool(L"ui",L"Backspace-unit-symbol"));
			_state = State::None;
			break ;
		case ChmFuncType::CompFinishKatakana: // カタカナ変換
			ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending, 
			_pConfig->GetBool(L"ui",L"Backspace-unit-symbol"));
			_converted = ChmRomajiConverter::HiraganaToKatakana(_converted);
			_state = State::None;
			break ;
		case ChmFuncType::CompFinish:     // 見たまま変換
			ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending,
				_pConfig->GetBool(L"ui",L"Backspace-unit-symbol"));
			_state = State::None;
			break;
		case ChmFuncType::CompFinishKey:    // ASCII確定
			_converted = _pRawInputStore->get();
			_pending = L"";
			_state = State::None;
			break;
		case ChmFuncType::CompFinishKeyWide: // 全角ASCII確定
			_converted = AsciiToWide(_pRawInputStore->get());
			_pending = L"";
			_state = State::None;
			break;
		case ChmFuncType::SelectInput:
			// 選択処理の場合
			if ( _state == State::Selecting) {
				// キーからindexを求める
				int index = _pL3Helper->KeyToIndex(keyEvent.GetChar());
				if (index < 0) {
					// indexが見つからない場合はエラー
					_pending = L"?idx";
				}
				else {
					uint32_t selected = _pL3KanjiSelect->SelectByIndex(index) ;
					if ( selected == 0) {
						// その位置に選択肢がない場合はエラー
						_pending = L"?ch";
					}
					else {
						// 選択成功の場合は確定して終了
						_converted = ChmL3Helper::Utf32ToWString(selected);
						_pending = L"";
						_state = State::None;
						// 選択オブジェクトはもう不要なので破棄
						delete _pL3KanjiSelect ;
						_pL3KanjiSelect = nullptr; 
					}
				}
			}
			break ;

		// --------
		// その他の機能キー
		case ChmFuncType::ReloadIni: {
			bool bRet = _pConfig->LoadFile();
			if (bRet) {
				_converted = L"ok";
			}else {
				_converted = L"ng";
			}
			_pending = L"";
			_state = State::Inputing;
			break;
		}
		case ChmFuncType::VersionInfo:
			_converted = HM_VERSION L"(" __DATE__ L" " __TIME__ L")";
			_pending = L"";
			_state = State::Inputing;
			break;

		default:
			// その他のキーは何もしない
			break;
	}
	pEndComposition = !HasComposition();

	return ;
}

void ChmEngine::PostUpdateComposition(){
	// 変換中でなくなった場合は、残りかすを処分
	if (!HasComposition()) {
		ResetStatus();
	}
	return;
}

void ChmEngine::ResetStatus() {
	_state = State::None;
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
