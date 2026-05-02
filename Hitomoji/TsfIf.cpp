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
#include "CEditSession.h"
#include "ChmKeyEvent.h"
#include "DisplayAttribute.h"
#include "ChmLangBar.h"
#include "ChmConfig.h"
#include "ChmEnvironment.h"

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
	ClearComposition();
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
		ClearComposition();
		_pEngine->ResetStatus();
	}
	return S_OK;
}

// ITfKeyEventSink Implementation
STDMETHODIMP ChmTsfInterface::OnTestKeyDown(ITfContext* pic, WPARAM wp, LPARAM lp, BOOL* pfEaten) {
	*pfEaten = _pEngine->IsKeyEaten(wp);
	ChmKeyEvent kEv(wp, lp,_pEngine->GetState());
	OutputDebugStringWithString(((std::wstring)L"[Hitomoji] OnTestKeyDown eat=%s" + kEv.toString()).c_str(), *pfEaten ? L"TRUE" : L"FALSE") ;

	return S_OK;
}

STDMETHODIMP ChmTsfInterface::OnKeyDown(ITfContext* pic, WPARAM wp, LPARAM lp, BOOL* pfEaten)
{
	OutputDebugString(L"[Hitomoji] OnKeyDown");
	*pfEaten = _pEngine->IsKeyEaten(wp);
	if (*pfEaten) {

		bool fEnd = false;
		ChmKeyEvent kEv(wp, lp,_pEngine->GetState());
		_pEngine->UpdateComposition(kEv,fEnd);
		_InvokeEditSession(pic, fEnd);
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
			bool fEnd = true;
			// OFFになる時にCompositionが残っている場合は、それを確定させる
			ChmKeyEvent kEv(ChmFuncType::CompFinish);
			_pEngine->UpdateComposition(kEv,fEnd);
			_InvokeEditSession(pic, fEnd);
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

HRESULT ChmTsfInterface::_InvokeEditSession(ITfContext* pic, BOOL fEnd) {
	// ---- pre-check: composition / context validity ----
	if (_pComposition) {
		// context mismatch -> clear
		if (_pContextForComposition != pic) {
			OutputDebugString(L"[Hitomoji] _InvokeEditSession: context mismatch -> clear");
			ClearComposition();
			_pEngine->ResetStatus();
		} else {
			// context match -> check view existence
			ITfCompositionView* pView = nullptr;
			HRESULT hrView = GetFirstCompositionView(pic, &pView);
			if (hrView != S_OK || !pView) {
				// TODO： OnEndEditが届かない場合、_pCompositionが初期化されずここに来る。でも、その検出手段がない。
				// 　　　 ここを通ると、エンジン側のRawInputがクリアされ、フォーカス移動直後の1文字が消失するが、
				// 　　　 現状では対抗手段がないので、この仕様を受容する。
				OutputDebugString(L"[Hitomoji] _InvokeEditSession: no view -> clear");
				ClearComposition();
				_pEngine->ResetStatus();
			} else {
				pView->Release();
			}
		}
	}

	// ---- edit session request ----
	HRESULT hr;
	CEditSessionBase* pES = nullptr;
	if (_pEngine->UseUndoEditSession()) {
		pES = new CUndoEditSession(pic, _tfClientId, this, _pEngine->GetCompositionStr(), _pEngine->GetUndoDeleteLength());
	} else {
		pES = new CEditSession(pic, _tfClientId, this, _pEngine->GetCompositionStr(), fEnd);
	}
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
	if (GetCompositionContext()) {
		ClearComposition();
		_pEngine->ResetStatus();
	}
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
		_InvokeEditSession(GetCompositionContext(), TRUE);
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
