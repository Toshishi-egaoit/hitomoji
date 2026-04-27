// TsfIf.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msctf.h>
#include <cstdio>
#include <string>
#include <cctype>
#include <initguid.h> // GUIDの実体定義用
#include <objbase.h>
#include <shellapi.h>
#include "TsfIf.h"
#include "ChmKeyEvent.h"
#include "DisplayAttribute.h"
#include "ChmLangBar.h"
#include "ChmConfig.h"
#include "ChmEnvironment.h"

class CEditSession : public ITfEditSession , public ITfCompositionSink{
public:

	CEditSession(
		ITfContext* pic, 
		ChmTsfInterface* pTsfIf,
		std::wstring compositionStr,
		BOOL fTermBefore,
		ChmTsfInterface::DisplayMode dispMode,
		BOOL fTermAfter )
		: _pic(pic), _pTsfIf(pTsfIf), _compositionStr(compositionStr), 
		  _fTermBefore(fTermBefore), _dispMode(dispMode), _fTermAfter(fTermAfter), _cRef(1) {
	}

	STDMETHODIMP QueryInterface(REFIID riid, void** ppv) {
		if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
			*ppv = this; AddRef(); return S_OK;
		}
		return E_NOINTERFACE;
	}
	STDMETHODIMP_(ULONG) AddRef() { return ++_cRef; }
	STDMETHODIMP_(ULONG) Release() { if (--_cRef == 0) { delete this; return 0; } return _cRef; }

// ----- ITfCompositionSink の実装
	STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition) {
		return S_OK;
	}

	// アプリへの文字列の挿入や、Compositionの終了処理などを行う
	STDMETHODIMP DoEditSession(TfEditCookie ec) {
		Info(Format(L"DoEditSession:>>%s<< termBefore=%d　termAfter=%d",_compositionStr.c_str(), _fTermBefore, _fTermAfter));
		ITfRange* pRange = nullptr;

		// 確定処理（CommitFinal)

		// ホントの確定時はCompositionを終了させる
		if (_fTermBefore) {
			ITfComposition* pComp = _pTsfIf->GetComposition();
			if (!pComp) {
				Warn(L"   > composition do not exist");
			}
			else {
				HRESULT hr = pComp->GetRange(&pRange);
				OUTPUT_HR_n_RETURN_ON_ERROR(L"   > GetRange", hr);
				if (SUCCEEDED(hr) && pRange) {
					_moveCarret(ec, pRange);
					_TerminateComposition(ec);
					// TODO: 一部アプリで入力中のキャレット位置が左端になる
				}
			}
		}

		// 新規／更新
		if (_dispMode != ChmTsfInterface::DisplayMode::NoNeed) {

			// Compositionの準備
			if (FAILED(_GetOrStartComposition(ec, &pRange))) return S_OK;

			// 確定処理／未確定表示（共通 SetText）
			if ( !_compositionStr.empty() ) {
				pRange->SetText(ec, TF_ST_CORRECTION, _compositionStr.c_str(), (LONG)_compositionStr.length());
			}

			// 未確定：末尾をアクティブにし、そこから左へ未確定範囲を構成
			LONG cch = (LONG)_compositionStr.length();
			LONG shifted = 0;
			pRange->ShiftEnd(ec, cch, &shifted, nullptr);

			if (_dispMode == ChmTsfInterface::DisplayMode::Committing) {
				// 仮確定なので下線を消す。何もしなくてOK。
			} else if (_dispMode == ChmTsfInterface::DisplayMode::Inputing) {
				// 入力中は波線を引く
				_ApplyDisplayAttribute(ec, pRange);
			} else if (_dispMode == ChmTsfInterface::DisplayMode::Selecting) {
				// 変換中は波線を引く 
				// TODO:背景色反転も必要だが、実装は後回し
				_ApplyDisplayAttribute(ec, pRange);
			} else {
				// NoNeedは前段で排除済み
			}

			if (pRange) pRange->Release();
			_pTsfIf->SetMyEditSessionTick(); 
		}
		
		if (_fTermAfter) {
			_TerminateComposition(ec);
		}

		return S_OK;
	}

private:

	// 既存のCompositionを取得するか、なければ新しく開始する（簡略版）
	HRESULT _GetOrStartComposition(TfEditCookie ec, ITfRange** ppRange) {
		if (!ppRange) return E_INVALIDARG;
		*ppRange = nullptr;

		// --- 既存 Composition があれば無条件で再利用 ---
		ITfComposition* pComp = _pTsfIf->GetComposition();
		if (pComp) {
			HRESULT hr = pComp->GetRange(ppRange);
			if (SUCCEEDED(hr) && *ppRange) {
				return S_OK;
			}
		}

		// --- 新規 Composition 開始 ---
		ITfInsertAtSelection* pInsert = nullptr;
		HRESULT hr = _pic->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsert);
		if (FAILED(hr) || !pInsert) {
			return E_FAIL;
		}

		hr = pInsert->InsertTextAtSelection(ec, TS_IAS_QUERYONLY, L"", 0, ppRange);
		pInsert->Release();
		if (FAILED(hr) || !*ppRange) {
			return E_FAIL;
		}

		ITfContextComposition* pCtxComp = nullptr;
		hr = _pic->QueryInterface(IID_ITfContextComposition, (void**)&pCtxComp);
		if (FAILED(hr) || !pCtxComp) {
			(*ppRange)->Release();
			*ppRange = nullptr;
			return E_FAIL;
		}

		ITfComposition* pNewComp = nullptr;
		hr = pCtxComp->StartComposition(ec, *ppRange, this, &pNewComp);
		pCtxComp->Release();

		if (FAILED(hr) || !pNewComp) {
			(*ppRange)->Release();
			*ppRange = nullptr;
			return E_FAIL;
		}

		_pTsfIf->SetComposition(_pic, pNewComp);
		return S_OK;
	}

	// 下線を引くための詳細実装を切り出す
	HRESULT _ApplyDisplayAttribute(TfEditCookie ec, ITfRange* pRange) {
		ITfProperty* pProp = nullptr;
		HRESULT hr;
		hr = _pic->GetProperty(GUID_PROP_ATTRIBUTE, &pProp);
		OUTPUT_HR_n_RETURN_ON_ERROR(L"GetProperty", hr);

		VARIANT var;
		VariantInit(&var);
		var.vt = VT_I4;
		var.lVal = (LONG)CDisplayAttributeInfo::GetAtom();
		hr = pProp->SetValue(ec, pRange, &var);
		OUTPUT_HR_n_RETURN_ON_ERROR(L"SetValue", hr);
		pProp->Release();

		return S_OK;
	}

	void _moveCarret(TfEditCookie ec, ITfRange* pRange) {
		// 確定：キャレットを末尾へ移動して Composition 終了
		pRange->Collapse(ec, TF_ANCHOR_END);

		// 【追加】アプリ側のキャレット位置をこの Range の場所に同期させる
		TF_SELECTION sel;
		sel.range = pRange;
		sel.style.ase = TF_AE_NONE; // アクティブな末尾
		sel.style.fInterimChar = FALSE;
		_pic->SetSelection(ec, 1, &sel);
	}

	void _TerminateComposition(TfEditCookie ec) {
		Info(L"   > TerminateComposition");
		ITfComposition* pComp = _pTsfIf->GetComposition();
		if (!pComp) return; // 事前に Clear されている可能性があるため防御
		pComp->EndComposition(ec);
		_pTsfIf->ClearComposition();
	}

	// メンバ変数
	ITfContext* _pic; 
	ChmTsfInterface* _pTsfIf;
	std::wstring _compositionStr;
	BOOL _fTermBefore;
	ChmTsfInterface::DisplayMode _dispMode;
	BOOL _fTermAfter;
	LONG _cRef;
};

// --- CTsfInterface クラスの実装 ---

ChmTsfInterface::ChmTsfInterface():
	_tfClientId (0),
	_cRef(1),
	_pThreadMgr(nullptr),
	_pComposition(nullptr),
	_pContextForComposition(nullptr),
	_dwThreadFocusSinkCookie(TF_INVALID_COOKIE),
	_dwTextEditSinkCookie(TF_INVALID_COOKIE),
	_llMyEditSessionTick(0)
{ 
	_pEngine = new ChmEngine();
	_pLangBarItem = nullptr;
}

ChmTsfInterface::~ChmTsfInterface() {
	delete _pEngine;
}

// IUnknown Implementation
STDMETHODIMP ChmTsfInterface::QueryInterface(REFIID riid, void** ppvObj) {
	if (!ppvObj) return E_INVALIDARG;
	*ppvObj = nullptr;
	if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor)) {
		*ppvObj = (ITfTextInputProcessor*)this;
	} else if (IsEqualIID(riid, IID_ITfTextEditSink)) {
		*ppvObj = (ITfTextEditSink*)this;
	} else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
		*ppvObj = (ITfKeyEventSink*)this;
	} else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider)) {
		*ppvObj = (ITfDisplayAttributeProvider*)this;
	} else if (IsEqualIID(riid, IID_ITfThreadFocusSink)) {
		*ppvObj = (ITfThreadFocusSink*)this;
	}
	if (*ppvObj) { AddRef(); return S_OK; }
	return E_NOINTERFACE;
}
STDMETHODIMP_(ULONG) ChmTsfInterface::AddRef() { return ++_cRef; }
STDMETHODIMP_(ULONG) ChmTsfInterface::Release() {
	if (--_cRef == 0) { delete this; return 0; }
	return _cRef;
}

// -----
// ----- ITfTextInputProcessor のイベント処理 -----
// -----

STDMETHODIMP ChmTsfInterface::Activate(ITfThreadMgr* ptm, TfClientId tid) {
	ChmLogger::Info(L"Activate() : Hitomoji " HM_VERSION L"(" __DATE__ L")" );
	_pThreadMgr = ptm;
	_pThreadMgr->AddRef();
	_tfClientId = tid;

	if (FAILED(_InitKeyEventSink())) return E_FAIL;
	if (FAILED(_InitPreservedKey())) return E_FAIL;
	if (FAILED(_InitDisplayAttributeInfo())) return E_FAIL;

	// ChmLangBarItemButtonの初期化
	_pLangBarItem = new ChmLangBarItemButton(GUID_HmLangBar, this);
	_pLangBarItem->AddToLangBar(_pThreadMgr);

	// ThreadFocusSinkの登録
	ITfSource* pSource = nullptr;
	if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfSource, (void**)&pSource))) {
		HRESULT hr = pSource->AdviseSink(IID_ITfThreadFocusSink, (ITfThreadFocusSink*)this,&_dwThreadFocusSinkCookie);
		OUTPUT_HR_ON_ERROR(L"Activate.AdviceSink",hr);
		pSource->Release();
	}

	// ChmEngine のしょきか
	_pEngine->Activate();

	return S_OK;
}

STDMETHODIMP ChmTsfInterface::Deactivate() {
	ChmLogger::Info(L"Deactivate()");
	_FlushComposition();
	if (_dwThreadFocusSinkCookie != TF_INVALID_COOKIE) {
		ITfSource* pSource = nullptr;
		if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfSource, (void**)&pSource))) {
			pSource->UnadviseSink(_dwThreadFocusSinkCookie);
			pSource->Release();
		}
		_dwThreadFocusSinkCookie = TF_INVALID_COOKIE;
	}
	_pEngine->Deactivate();

	// ChmLangBarItemButtonの後始末
	_pLangBarItem->RemoveFromLangBar(_pThreadMgr);
	_pLangBarItem->Release();
	_pLangBarItem = nullptr;

	_UninitKeyEventSink();
	_UninitPreservedKey();
	if (_pThreadMgr) {
		_pThreadMgr->Release();
		_pThreadMgr = nullptr;
	}
	return S_OK;
}

HRESULT ChmTsfInterface::GetFirstCompositionView(
	ITfContext* pic,
	ITfCompositionView** ppView)
{
	if (!pic || !ppView) return E_INVALIDARG;
	*ppView = nullptr;

	ITfContextComposition* pCtxComp = nullptr;
	HRESULT hr = pic->QueryInterface(
		IID_ITfContextComposition,
		(void**)&pCtxComp);

	if (FAILED(hr) || !pCtxComp) {
		return hr;
	}

	IEnumITfCompositionView* pEnum = nullptr;
	hr = pCtxComp->EnumCompositions(&pEnum);
	pCtxComp->Release();

	if (FAILED(hr) || !pEnum) {
		return hr;
	}

	ITfCompositionView* pView = nullptr;
	ULONG fetched = 0;
	hr = pEnum->Next(1, &pView, &fetched);
	pEnum->Release();

	if (hr == S_OK && fetched == 1 && pView) {
		*ppView = pView; // AddRef 済み
		return S_OK;
	}

	// Composition なし
	if (pView) {
		pView->Release();
	}
	return S_FALSE;
}


// -----
// ----- ITfKeyEventSink  のイベント処理 -----
// -----

STDMETHODIMP ChmTsfInterface::OnSetFocus(BOOL fFocus)
{
	return S_OK;
	if (fFocus == TRUE) { // フォーカス取得時
		_FlushComposition();
	}
	return S_OK;
}

// Compositionがある場合、EndCompositionし、てCompositionの矛盾を防ぐ。
// フォーカス移動、Eatenしないキー入力時、アプリ切り替え時、Deactivate時に呼び出す。
void ChmTsfInterface::_FlushComposition() {
	Info(Format(L"_FlushComposition:pComposition=%x state=%d",_pComposition, _pEngine->GetState()));

	if ( _pComposition == nullptr ) return ;

	ITfRange* pRange = nullptr ;
	if (FAILED(_pComposition->GetRange(&pRange))) {
		Info(L"   > GetRange failed");
		ClearComposition();
		_pEngine->ResetStatus();
		return ;
	}
	pRange->Release();

	ITfContext* pic = GetCompositionContext();
	// 終了前、入力中の見た目、終了後はそのまま
	BOOL fTerminateCompBefore = (_pEngine->GetState() == ChmEngine::State::Committing);
	BOOL fTerminateCompAfter = (_pEngine->GetState() == ChmEngine::State::Inputing) || (_pEngine->GetState() == ChmEngine::State::Selecting);
	DisplayMode dispMode ;
	
	switch (_pEngine->GetState()) {
		case ChmEngine::State::NoNeed: 
			dispMode = ChmTsfInterface::DisplayMode::NoNeed; 
			break;
		case ChmEngine::State::Inputing: 
		case ChmEngine::State::Selecting:
			dispMode = ChmTsfInterface::DisplayMode::Committing;
			break;
		case ChmEngine::State::Committing:
			dispMode = ChmTsfInterface::DisplayMode::NoNeed; 
	}
	
	CEditSession* pES = new CEditSession(pic, this, 
		_pEngine->GetCompositionStr(), 
		fTerminateCompBefore, dispMode, fTerminateCompAfter);
	if (!pES) return ;
	HRESULT hr;
	HRESULT hrSession = pic->RequestEditSession(_tfClientId, pES, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
	OUTPUT_HR_ON_ERROR("   > RequestEditSession", hrSession);
	pES->Release();

	return ;
}

// ITfKeyEventSink Implementation
STDMETHODIMP ChmTsfInterface::OnTestKeyDown(ITfContext* pic, WPARAM wp, LPARAM lp, BOOL* pfEaten) {
	*pfEaten = _pEngine->IsKeyEaten(wp);
	ChmKeyEvent kEv(wp, lp,_pEngine->GetState());
	Info(Format(L"OnTestKeyDown eat=%s %s", kEv.toString().c_str(), *pfEaten ? L"TRUE" : L"FALSE"));

	return S_OK;
}

STDMETHODIMP ChmTsfInterface::OnKeyDown(ITfContext* pic, WPARAM wp, LPARAM lp, BOOL* pfEaten)
{
	Info(L"OnKeyDown");
	*pfEaten = _pEngine->IsKeyEaten(wp);
	if (*pfEaten) {
		bool fTerminateCompBefore = false;
		bool fTerminateCompAfter = false;
		ChmKeyEvent kEv(wp, lp,_pEngine->GetState());
		_pEngine->UpdateComposition(kEv, fTerminateCompBefore, fTerminateCompAfter);
		_InvokeEditSession(pic, fTerminateCompBefore, _GetDisplayMode(kEv.GetType()), fTerminateCompAfter);
		_pEngine->PostUpdateComposition();
	}
	return S_OK;
}

STDMETHODIMP ChmTsfInterface::OnTestKeyUp(ITfContext* pic, WPARAM wp, LPARAM lp, BOOL* pfEaten) {
	*pfEaten = FALSE;
	return S_OK;
}

STDMETHODIMP ChmTsfInterface::OnKeyUp(ITfContext* pic, WPARAM wp, LPARAM lp, BOOL* pfEaten) {
	*pfEaten = FALSE;
	return S_OK;
}

STDMETHODIMP ChmTsfInterface::OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten) {
	if (IsEqualGUID(rguid, GUID_PreservedKey_OpenClose)) {
		if ( _pEngine->IsON() && _pEngine->HasComposition() ) {
			bool fEndBefore = true;
			bool fEndAfter = false;
			// OFFになる時にCompositionが残っている場合は、それを確定させる
			ChmKeyEvent kEv(ChmFuncType::CompFinish);
			_pEngine->UpdateComposition(kEv,fEndBefore, fEndAfter);
			_InvokeEditSession(pic, fEndBefore, _GetDisplayMode(ChmFuncType::CompFinish), fEndAfter);
			_pEngine->PostUpdateComposition();
		}

		ToggleIME();
		OutputDebugString(L"[Hitomoji]OnPreservedKey:processed");
		*pfEaten = TRUE;
	}
	return S_OK;
}

HRESULT ChmTsfInterface::_InitKeyEventSink() {
	ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
	HRESULT hr = _pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr);
	if (SUCCEEDED(hr)) {
		hr = pKeystrokeMgr->AdviseKeyEventSink(_tfClientId, (ITfKeyEventSink*)this, TRUE);
		pKeystrokeMgr->Release();
	}
	return hr;
}

void ChmTsfInterface::_UninitKeyEventSink() {
	ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
	if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr))) {
		pKeystrokeMgr->UnadviseKeyEventSink(_tfClientId);
		pKeystrokeMgr->Release();
	}
}

HRESULT ChmTsfInterface::_InitPreservedKey() {
	ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
	HRESULT hr = _pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr);
	if (SUCCEEDED(hr)) {
		hr = pKeystrokeMgr->PreserveKey(_tfClientId, GUID_PreservedKey_OpenClose, &c_presKeyOpenClose, L"ひともじ ON/OFF", (ULONG)wcslen(L"ひともじ ON/OFF"));
		pKeystrokeMgr->Release();
	}
	return hr;
}

void ChmTsfInterface::_UninitPreservedKey() {
	ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
	if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr))) {
		pKeystrokeMgr->UnpreserveKey(GUID_PreservedKey_OpenClose, &c_presKeyOpenClose);
		pKeystrokeMgr->Release();
	}
}

HRESULT ChmTsfInterface::_InvokeEditSession(ITfContext* pic, BOOL fTerminateCompBefore, ChmTsfInterface::DisplayMode dispMode, BOOL fTerminateCompAfter) {
	// ---- pre-check: composition / context validity ----
	Info(Format(L"_InfokeEditSession:pComposition=%x fEndBefore=%d fEndAfter=%d",_pComposition , fTerminateCompBefore, fTerminateCompAfter));
	if (_pComposition) {
		// context mismatch -> clear
		if (_pContextForComposition != pic) {
			Info(L"_InvokeEditSession: context mismatch -> clear");
			_FlushComposition();
		} else {
			// context match -> check view existence
			ITfCompositionView* pView = nullptr;
			HRESULT hrView = GetFirstCompositionView(pic, &pView);
			if (hrView != S_OK || !pView) {
				// TODO： OnEndEditが届かない場合、_pCompositionが初期化されずここに来る。でも、その検出手段がない。
				// 　　　 ここを通ると、エンジン側のRawInputがクリアされ、フォーカス移動直後の1文字が消失するが、
				// 　　　 現状では対抗手段がないので、この仕様を受容する。
				Info(L"_InvokeEditSession: no view -> clear");
				_FlushComposition();
			} else {
				pView->Release();
			}

		}
	}

	// ---- normal edit session request ----
	HRESULT hr;
	CEditSession* pES = new CEditSession(pic, this, 
		_pEngine->GetCompositionStr(), 
		fTerminateCompBefore,
		dispMode,
		fTerminateCompAfter
		); 
	if (!pES) return E_OUTOFMEMORY;
	HRESULT hrSession = pic->RequestEditSession(_tfClientId, pES, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
	OUTPUT_HR_ON_ERROR("_InvokeEditSession/RequestEditSession", hrSession);
	pES->Release();
	return hrSession;
}

// -----
// ----- ITfDisplayAttributeProvider  のイベント処理 -----
// -----

STDMETHODIMP ChmTsfInterface::GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo **ppInfo) {
	if (CDisplayAttributeInfo::IsMyGuid(guid)) {
		*ppInfo = new CDisplayAttributeInfo(); // さっき作ったクラスを投げる
		return S_OK;
	}
	return E_INVALIDARG;
}

STDMETHODIMP ChmTsfInterface::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo **ppEnum) {
	if (ppEnum == nullptr) return E_INVALIDARG;
	*ppEnum = nullptr;
	
	// TODO: 将来的に複数属性をサポートする場合はここを実装する。
	// v0.1 では一旦「未実装」でOK。
	// エディタは GetDisplayAttributeInfo さえ呼べれば描画できます。
	return E_NOTIMPL; 
}

HRESULT ChmTsfInterface::_InitDisplayAttributeInfo()
{
	ITfCategoryMgr* pCategoryMgr = nullptr;
	HRESULT hr = CoCreateInstance(
		CLSID_TF_CategoryMgr,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_ITfCategoryMgr,
		(void**)&pCategoryMgr
	);
	OUTPUT_HR_n_RETURN_ON_ERROR("_InitDisplayAttributeInfo/CoCreateInstance",hr);

	hr = CDisplayAttributeInfo::InitGuid(pCategoryMgr);
	pCategoryMgr->Release();
	return hr;
}

void ChmTsfInterface::_UninitDisplayAttributeInfo() { 
	// TODO:ちゃんと解放処理を書く
	return ; 
}

// -----
// ----- ITfThreadFocusSink  のイベント処理 -----
// -----

STDMETHODIMP ChmTsfInterface::OnSetThreadFocus()
{
	OutputDebugString(L"[Hitomoji] OnSetThreadFocus()");

	// フォーカス取得時は Composition 側のみを基準に整理
	_FlushComposition();

	return S_OK;
}


STDMETHODIMP ChmTsfInterface::OnKillThreadFocus() {
	OutputDebugString(L"OnKillThreadFocus()");
	return S_OK;
}

// -----
// ----- ITfTextEdit のイベント処理 -----
// -----

STDMETHODIMP ChmTsfInterface::OnEndEdit(
	ITfContext* pic,
	TfEditCookie ecReadOnly,
	ITfEditRecord* pEditRecord)
{
	OutputDebugString(L"[Hitomoji] OnEndEdit()");

	// 編集中でなければ何もしない
	if (_pComposition == nullptr) return S_OK;
	// 自分の EditSession 由来なら無視
	if (IsMyEditSession()) return S_OK;

	// Composition の属する Context と異なる編集が入った場合のみ確定
	if (_pContextForComposition && pic != GetCompositionContext()) {
		OutputDebugString(L"   > InvokeEditSession via composition context");
		_InvokeEditSession(GetCompositionContext(), TRUE, ChmTsfInterface::DisplayMode::NoNeed, TRUE);
		_pEngine->ResetStatus();
	}
	return S_OK;
}

BOOL ChmTsfInterface::ToggleIME() 
{
	if (!_pEngine) return FALSE;

	// ロジック上のIME状態を切り替え
	_pEngine->ToggleIME();
	BOOL newState = _pEngine->IsON();

	// OSに状態通知
	_SetImeOpenClose(newState);

	// LangBarの状態update
	if (_pLangBarItem) {
		_pLangBarItem->SetImeState(newState);
	}
	return TRUE;
}

ChmTsfInterface::DisplayMode ChmTsfInterface::_GetDisplayMode(ChmFuncType fType) {
	switch (fType) {
		case ChmFuncType::CharInput:
			return ChmTsfInterface::DisplayMode::Inputing;
		case ChmFuncType::CharInputSpace:
			return ChmTsfInterface::DisplayMode::Committing;
		case ChmFuncType::CompFinish:         // 見たまま確定(ENTER)
		case ChmFuncType::CompFinishHiragana: // ひらがな確定(Alt+Enter)
		case ChmFuncType::CompFinishKatakana: // カタカナ確定(Shift+Enter)
		case ChmFuncType::CompFinishKey:      // キーどおり確定(TAB)
		case ChmFuncType::CompFinishKeyWide:  // キーどおり確定ワイド(Shift+TAB)
			return ChmTsfInterface::DisplayMode::Committing;
		case ChmFuncType::CompSelect:         // 選択開始(かな漢字変換)
			return ChmTsfInterface::DisplayMode::Selecting;
		case ChmFuncType::SelectInput:        // 選択
			return ChmTsfInterface::DisplayMode::Committing;
		case ChmFuncType::SelectNextPage:     // 選択中の次ページ
		case ChmFuncType::SelectPrevPage:     // 選択中の次ページ
			return ChmTsfInterface::DisplayMode::Selecting;
		case ChmFuncType::SelectCancel:       // 選択中のキャンセル
			return ChmTsfInterface::DisplayMode::Inputing;
		case ChmFuncType::Cancel:             // キャンセル(ESC)
		case ChmFuncType::Backspace:          // 後退(BS)
		case ChmFuncType::UnFinish:           // 確定取消(CTRL+Z)
		case ChmFuncType::VersionInfo:        // バージョン表示
			return ChmTsfInterface::DisplayMode::Inputing;
		default:
			return ChmTsfInterface::DisplayMode::NoNeed;
	};
}

// IME ON/OFF を OS に通知する
void ChmTsfInterface::_SetImeOpenClose(BOOL fOpen)
{
	if (!_pThreadMgr) return;

	ITfCompartmentMgr* pCompMgr = nullptr;
	if (FAILED(_pThreadMgr->QueryInterface(IID_ITfCompartmentMgr, (void**)&pCompMgr)))
		return;

	ITfCompartment* pComp = nullptr;
	if (SUCCEEDED(pCompMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &pComp)))
	{
		VARIANT var;
		VariantInit(&var);
		var.vt = VT_I4;
		var.lVal = fOpen ? 1 : 0;   // 1=IME ON, 0=IME OFF
		pComp->SetValue(0, &var);
		pComp->Release();
	}
	pCompMgr->Release();
}

HRESULT ChmTsfInterface::_InitTextEditSink(ITfContext* pic)
{
	ITfSource* pSource = nullptr;
	if (FAILED(pic->QueryInterface(IID_ITfSource, (void**)&pSource)))
		return E_FAIL;

	HRESULT hr = pSource->AdviseSink(
		IID_ITfTextEditSink,
		static_cast<ITfTextEditSink*>(this),
		&_dwTextEditSinkCookie
	);

	pSource->Release();
	return hr;
}

HRESULT ChmTsfInterface::_UninitTextEditSink(ITfContext* pic)
{
	if (!pic) return S_OK;
	if (_dwTextEditSinkCookie == TF_INVALID_COOKIE) return S_OK;

	ITfSource* pSource = nullptr;
	if (SUCCEEDED(pic->QueryInterface(IID_ITfSource, (void**)&pSource))) {
		pSource->UnadviseSink(_dwTextEditSinkCookie);
		pSource->Release();
	}

	_dwTextEditSinkCookie = TF_INVALID_COOKIE;
	return S_OK;
}

// --- configファイルを開く ---
void ChmTsfInterface::OpenConfig()
{
	std::wstring configPath = _pEngine->GetConfigFile();
	HINSTANCE h = ShellExecuteW(
		NULL,
		L"open",
		configPath.c_str(),
		NULL,
		NULL,
		SW_SHOWNORMAL
	);

	if ((INT_PTR)h <= 32)
	{
		ChmLogger::Warn(L"OpenConfig failed");
	}
}

void ChmTsfInterface::ReloadConfig() {
	_pEngine->InitConfig();
	return;
}

void ChmTsfInterface::OpenFolder()
{
	ShellExecuteW(
		NULL,
		L"open",
		g_environment.GetBasePath().c_str(),
		NULL,
		NULL,
		SW_SHOWNORMAL
	);
	return;
}
