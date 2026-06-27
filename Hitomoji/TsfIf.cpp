// TsfIf.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msctf.h>
#include <cstdio>
#include <string>
#include <cctype>
#include <initguid.h> // GUIDの実体定義用
#include <objbase.h>
#include <inputscope.h>
#include <shellapi.h>
#include "TsfIf.h"
#include "CEditSession.h"
#include "ChmKeyEvent.h"
#include "DisplayAttribute.h"
#include "ChmLangBar.h"
#include "ChmConfig.h"
#include "ChmEnvironment.h"
#include "ChmCandidateWindow.h"

namespace {
BOOL IsDbgViewProcess()
{
	WCHAR path[MAX_PATH] = {};
	DWORD len = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
	if (len == 0 || len >= ARRAYSIZE(path)) return FALSE;

	LPCWSTR fileName = path;
	for (LPCWSTR p = path; *p; ++p) {
		if (*p == L'\\' || *p == L'/') {
			fileName = p + 1;
		}
	}
	ChmLogger::Info(Format(L"process name: %s", fileName));

	return lstrcmpiW(fileName, L"Dbgview.exe") == 0 ||
		lstrcmpiW(fileName, L"Dbgview64.exe") == 0 ||
		lstrcmpiW(fileName, L"Dbgview64a.exe") == 0;
}
}

// --- CTsfInterface クラスの実装 ---

ChmTsfInterface::ChmTsfInterface():
	_tfClientId (0),
	_cRef(1),
	_pThreadMgr(nullptr),
	_pComposition(nullptr),
	_pContextForComposition(nullptr),
	_dwThreadFocusSinkCookie(TF_INVALID_COOKIE),
	_dwThreadMgrEventSinkCookie(TF_INVALID_COOKIE),
	_dwKeyboardOpenCloseSinkCookie(TF_INVALID_COOKIE),
	_dwKeyboardDisabledSinkCookie(TF_INVALID_COOKIE),
	_dwEmptyContextSinkCookie(TF_INVALID_COOKIE),
	_dwTextEditSinkCookie(TF_INVALID_COOKIE),
	_llMyEditSessionTick(0),
	_isSettingOpenClose(FALSE),
	_pCandidateWindowThread(nullptr),
	_candidateWindowPosition{},
	_hasCandidateWindowPosition(FALSE),
	_isDisabledForProcess(FALSE)
{ 
	InitializeCriticalSection(&_candidateWindowPositionLock);
	_pEngine = new ChmEngine();
	_pLangBarItem = nullptr;
}

ChmTsfInterface::~ChmTsfInterface() {
	if (_pCandidateWindowThread) {
		_pCandidateWindowThread->Stop();
		delete _pCandidateWindowThread;
		_pCandidateWindowThread = nullptr;
	}
	delete _pEngine;
	DeleteCriticalSection(&_candidateWindowPositionLock);
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
	} else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
		*ppvObj = (ITfThreadMgrEventSink*)this;
	} else if (IsEqualIID(riid, IID_ITfCompartmentEventSink)) {
		*ppvObj = (ITfCompartmentEventSink*)this;
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
	if (IsDbgViewProcess()) {
		_isDisabledForProcess = TRUE;
		return S_OK;
	}

	g_environment.Init();
	ChmLogger::Info(L"Activate() : Hitomoji " HM_VERSION L"(" __DATE__ L")" );
	_pThreadMgr = ptm;
	_pThreadMgr->AddRef();
	_tfClientId = tid;

	if (FAILED(_InitKeyEventSink())) return E_FAIL;
	if (FAILED(_InitPreservedKey())) return E_FAIL;
	if (FAILED(_InitDisplayAttributeInfo())) return E_FAIL;
	if (FAILED(_InitThreadMgrEventSink())) return E_FAIL;
	if (FAILED(_InitKeyboardOpenCloseSink())) return E_FAIL;

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

	// 候補ウィンドウ表示スレッドの初期化
	_pCandidateWindowThread = new ChmCandidateWindowThread(_GetCandidateWindowPositionCallback, this);
	if (!_pCandidateWindowThread || !_pCandidateWindowThread->Start(g_hInst)) {
		ChmLogger::Warn(L"Candidate window thread start failed");
		delete _pCandidateWindowThread;
		_pCandidateWindowThread = nullptr;
	}

	return S_OK;
}

STDMETHODIMP ChmTsfInterface::Deactivate() {
	if (_isDisabledForProcess) {
		_isDisabledForProcess = FALSE;
		return S_OK;
	}

	ChmLogger::Info(L"Deactivate()");
	ClearComposition();
	if (_pCandidateWindowThread) {
		_pCandidateWindowThread->Stop();
		delete _pCandidateWindowThread;
		_pCandidateWindowThread = nullptr;
	}
	if (_dwThreadFocusSinkCookie != TF_INVALID_COOKIE) {
		ITfSource* pSource = nullptr;
		if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfSource, (void**)&pSource))) {
			pSource->UnadviseSink(_dwThreadFocusSinkCookie);
			pSource->Release();
		}
		_dwThreadFocusSinkCookie = TF_INVALID_COOKIE;
	}
	_UninitKeyboardOpenCloseSink();
	_UninitThreadMgrEventSink();
	_pEngine->Deactivate();

	// ChmLangBarItemButtonの後始末
	if (_pLangBarItem) {
		_pLangBarItem->RemoveFromLangBar(_pThreadMgr);
		_pLangBarItem->Release();
		_pLangBarItem = nullptr;
	}

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
	if (fFocus == TRUE) { // フォーカス取得時
		ITfContext* pContext = nullptr;
		if (SUCCEEDED(_GetFocusedContext(&pContext)) && pContext) {
			_ApplyAppInputMode(pContext);
			pContext->Release();
		}
	}
	return S_OK;
}

// ITfKeyEventSink Implementation
STDMETHODIMP ChmTsfInterface::OnTestKeyDown(ITfContext* pic, WPARAM wp, LPARAM lp, BOOL* pfEaten) {
	*pfEaten = _pEngine->IsKeyEaten(wp);
	if (!*pfEaten) {
		_pEngine->InvalidateUnFinishByKey(wp);
	}
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
		ChmFuncType type = kEv.GetType();
		ChmCandidatePage page{};
		BOOL hasCandidatePage = FALSE;

		if (ChmEngine::IsLayer3Function(type)) {
			if (!_pEngine->UpdateLayer3(kEv, fEnd, &page)) return S_OK;
			hasCandidatePage = page.candidateCount > 0;
		} else if (ChmEngine::IsLayer2Function(type)) {
			if (!_pEngine->UpdateLayer2(kEv, fEnd)) return S_OK;
		} else {
			if (!_pEngine->UpdateOther(kEv, fEnd)) return S_OK;
		}

		_InvokeEditSession(pic, fEnd);
		_pEngine->PostUpdateComposition();
		_HandleEngineError(pic);

		if (_pCandidateWindowThread) {
			if (hasCandidatePage && _pEngine->GetState() == ChmEngine::State::Selecting) {
				DWORD delayMs = _pEngine->GetConfig()->GetLong(L"ui", L"candidate-delay", 500);
				_pCandidateWindowThread->ScheduleShow(page, delayMs);
			} else if (_pEngine->GetState() != ChmEngine::State::Selecting) {
				_pCandidateWindowThread->Hide();
			}
		}
	}
	return S_OK;
}

void ChmTsfInterface::_HandleEngineError(ITfContext* pic)
{
	if (!_pEngine->ConsumeErrorRequest()) return;

	if (_pEngine->GetConfig()->GetBool(L"ui", L"error-beep", TRUE)) {
		MessageBeep(MB_OK);
	} else {
		_TriggerVisualBell(pic);
	}
}

void ChmTsfInterface::_TriggerVisualBell(ITfContext* /*pic*/)
{
	// TODO: VisualBellとしてCompositionの表示属性を短時間だけ反転させる。
	//       期間は ui.visual-bell-ms（既定300ms）で制御する想定。
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
		if (_pCandidateWindowThread) {
			_pCandidateWindowThread->Hide();
		}

		// OFFになる時にCompositionが残っている場合は、それを確定させる
		_CommitComposition(pic);

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

BOOL ChmTsfInterface::_GetCandidateWindowPositionCallback(void* context, POINT& pt)
{
	if (!context) return FALSE;
	return static_cast<ChmTsfInterface*>(context)->GetCandidateWindowPosition(pt);
}

HRESULT ChmTsfInterface::_CommitComposition(ITfContext* pic) {
	Debug(Format(L"_CommitComposition: pic=%p", pic));
	if (!pic) return S_FALSE;
	if (!_pEngine->IsON() || !_pEngine->HasComposition()) return S_FALSE;

	bool fEnd = true;
	ChmKeyEvent kEv(ChmFuncType::CompFinish);
	if (!_pEngine->UpdateComposition(kEv, fEnd)) return S_FALSE;

	HRESULT hr = _InvokeEditSession(pic, TRUE);
	_pEngine->PostUpdateComposition();
	return hr;
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

	if (_pCandidateWindowThread) {
		_pCandidateWindowThread->Hide();
	}

	ITfContext* pContext = nullptr;
	if (SUCCEEDED(_GetFocusedContext(&pContext))) {
		_ResetCompositionOnFocusChange(pContext, L"OnSetThreadFocus");
	}
	if (pContext) {
		_ApplyAppInputMode(pContext);
		pContext->Release();
	}
	return S_OK;
}


STDMETHODIMP ChmTsfInterface::OnKillThreadFocus() {
	OutputDebugString(L"OnKillThreadFocus()");
	if (_pCandidateWindowThread) {
		_pCandidateWindowThread->Hide();
	}
	if (GetCompositionContext()) {
		ClearComposition();
		_pEngine->ResetStatus();
	}
	return S_OK;
}

// -----
// ----- ITfThreadMgrEventSink のイベント処理 -----
// -----

STDMETHODIMP ChmTsfInterface::OnInitDocumentMgr(ITfDocumentMgr* /*pdim*/)
{
	return S_OK;
}

STDMETHODIMP ChmTsfInterface::OnUninitDocumentMgr(ITfDocumentMgr* /*pdim*/)
{
	return S_OK;
}

STDMETHODIMP ChmTsfInterface::OnSetFocus(ITfDocumentMgr* pdimFocus, ITfDocumentMgr* /*pdimPrevFocus*/)
{
	OutputDebugString(L"[Hitomoji] ITfThreadMgrEventSink::OnSetFocus()");

	if (_pCandidateWindowThread) {
		_pCandidateWindowThread->Hide();
	}
	ITfContext* pContext = nullptr;
	if (SUCCEEDED(_GetTopContext(pdimFocus, &pContext))) {
		_ResetCompositionOnFocusChange(pContext, L"OnSetFocus");
	}
	if (pContext) {
		_ApplyAppInputMode(pContext);
		pContext->Release();
	}
	return S_OK;
}

BOOL ChmTsfInterface::_IsSameContext(ITfContext* pLeft, ITfContext* pRight)
{
	if (!pLeft || !pRight) return FALSE;
	if (pLeft == pRight) return TRUE;

	IUnknown* pUnkLeft = nullptr;
	IUnknown* pUnkRight = nullptr;
	HRESULT hrLeft = pLeft->QueryInterface(IID_IUnknown, (void**)&pUnkLeft);
	HRESULT hrRight = pRight->QueryInterface(IID_IUnknown, (void**)&pUnkRight);
	BOOL isSame = SUCCEEDED(hrLeft) && SUCCEEDED(hrRight) && pUnkLeft == pUnkRight;

	if (pUnkLeft) pUnkLeft->Release();
	if (pUnkRight) pUnkRight->Release();
	return isSame;
}

void ChmTsfInterface::_ResetCompositionOnFocusChange(ITfContext* pNewContext, LPCWSTR source)
{
	ITfContext* pCompositionContext = GetCompositionContext();
	if (!pCompositionContext) return;

	if (_IsSameContext(pCompositionContext, pNewContext)) {
		Debug(Format(L"%s: keep composition on same context", source ? source : L"focus"));
		return;
	}

	Debug(Format(L"%s: context changed -> clear composition", source ? source : L"focus"));
	ClearComposition();
	_pEngine->ResetStatus();
}

STDMETHODIMP ChmTsfInterface::OnPushContext(ITfContext* pic)
{
	_ApplyAppInputMode(pic);
	return S_OK;
}

STDMETHODIMP ChmTsfInterface::OnPopContext(ITfContext* /*pic*/)
{
	return S_OK;
}

// -----
// ----- ITfCompartmentEventSink のイベント処理 -----
// -----

STDMETHODIMP ChmTsfInterface::OnChange(REFGUID rguid)
{
	if (!IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE) &&
		!IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_DISABLED) &&
		!IsEqualGUID(rguid, GUID_COMPARTMENT_EMPTYCONTEXT)) {
		return S_OK;
	}
	if (_isSettingOpenClose) return S_OK;

	ITfContext* pContext = nullptr;
	if (SUCCEEDED(_GetFocusedContext(&pContext))) {
		_ApplyAppInputMode(pContext);
		if (pContext) pContext->Release();
	}
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
	if (_GetCompartmentBool(GUID_COMPARTMENT_KEYBOARD_DISABLED, FALSE) ||
		_GetCompartmentBool(GUID_COMPARTMENT_EMPTYCONTEXT, FALSE)) {
		_SetImeOpenState(FALSE, nullptr, FALSE);
		return FALSE;
	}

	ITfContext* pContext = nullptr;
	if (SUCCEEDED(_GetFocusedContext(&pContext)) && pContext) {
		if (_IsPasswordContext(pContext)) {
			pContext->Release();
			_SetImeOpenState(FALSE, nullptr, TRUE);
			return FALSE;
		}
		pContext->Release();
	}
	_SetImeOpenState(!_pEngine->IsON(), nullptr, TRUE);
	return TRUE;
}

void ChmTsfInterface::_SetImeOpenState(BOOL fOpen, ITfContext* pic, BOOL fNotifyOs)
{
	if (!_pEngine) return;

	if (!fOpen) {
		if (_pCandidateWindowThread) {
			_pCandidateWindowThread->Hide();
		}

		if (pic && GetCompositionContext()) {
			_CommitComposition(pic);
		} else if (GetCompositionContext()) {
			ClearComposition();
			_pEngine->ResetStatus();
		} else {
			_pEngine->ResetStatus();
		}
	}

	_pEngine->SetIME(fOpen);

	if (fNotifyOs) {
		_SetImeOpenClose(fOpen);
	}

	if (_pLangBarItem) {
		_pLangBarItem->SetImeState(fOpen);
	}
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
		_isSettingOpenClose = TRUE;
		pComp->SetValue(0, &var);
		_isSettingOpenClose = FALSE;
		pComp->Release();
	}
	pCompMgr->Release();
}

HRESULT ChmTsfInterface::_InitThreadMgrEventSink()
{
	if (!_pThreadMgr) return E_FAIL;

	ITfSource* pSource = nullptr;
	HRESULT hr = _pThreadMgr->QueryInterface(IID_ITfSource, (void**)&pSource);
	if (FAILED(hr)) return hr;

	hr = pSource->AdviseSink(
		IID_ITfThreadMgrEventSink,
		static_cast<ITfThreadMgrEventSink*>(this),
		&_dwThreadMgrEventSinkCookie
	);
	pSource->Release();
	return hr;
}

void ChmTsfInterface::_UninitThreadMgrEventSink()
{
	if (!_pThreadMgr || _dwThreadMgrEventSinkCookie == TF_INVALID_COOKIE) return;

	ITfSource* pSource = nullptr;
	if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfSource, (void**)&pSource))) {
		pSource->UnadviseSink(_dwThreadMgrEventSinkCookie);
		pSource->Release();
	}
	_dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
}

HRESULT ChmTsfInterface::_InitKeyboardOpenCloseSink()
{
	HRESULT hr = _InitCompartmentSink(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &_dwKeyboardOpenCloseSinkCookie);
	if (FAILED(hr)) return hr;

	hr = _InitCompartmentSink(GUID_COMPARTMENT_KEYBOARD_DISABLED, &_dwKeyboardDisabledSinkCookie);
	if (FAILED(hr)) return hr;

	return _InitCompartmentSink(GUID_COMPARTMENT_EMPTYCONTEXT, &_dwEmptyContextSinkCookie);
}

void ChmTsfInterface::_UninitKeyboardOpenCloseSink()
{
	_UninitCompartmentSink(GUID_COMPARTMENT_EMPTYCONTEXT, &_dwEmptyContextSinkCookie);
	_UninitCompartmentSink(GUID_COMPARTMENT_KEYBOARD_DISABLED, &_dwKeyboardDisabledSinkCookie);
	_UninitCompartmentSink(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &_dwKeyboardOpenCloseSinkCookie);
}

HRESULT ChmTsfInterface::_InitCompartmentSink(REFGUID rguid, DWORD* pdwCookie)
{
	if (!_pThreadMgr || !pdwCookie) return E_FAIL;
	if (*pdwCookie != TF_INVALID_COOKIE) return S_OK;

	ITfCompartmentMgr* pCompMgr = nullptr;
	HRESULT hr = _pThreadMgr->QueryInterface(IID_ITfCompartmentMgr, (void**)&pCompMgr);
	if (FAILED(hr)) return hr;

	ITfCompartment* pComp = nullptr;
	hr = pCompMgr->GetCompartment(rguid, &pComp);
	pCompMgr->Release();
	if (FAILED(hr)) return hr;

	ITfSource* pSource = nullptr;
	hr = pComp->QueryInterface(IID_ITfSource, (void**)&pSource);
	if (SUCCEEDED(hr)) {
		hr = pSource->AdviseSink(
			IID_ITfCompartmentEventSink,
			static_cast<ITfCompartmentEventSink*>(this),
			pdwCookie
		);
		pSource->Release();
	}
	pComp->Release();
	return hr;
}

void ChmTsfInterface::_UninitCompartmentSink(REFGUID rguid, DWORD* pdwCookie)
{
	if (!_pThreadMgr || !pdwCookie || *pdwCookie == TF_INVALID_COOKIE) return;

	ITfCompartmentMgr* pCompMgr = nullptr;
	if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfCompartmentMgr, (void**)&pCompMgr))) {
		ITfCompartment* pComp = nullptr;
		if (SUCCEEDED(pCompMgr->GetCompartment(rguid, &pComp))) {
			ITfSource* pSource = nullptr;
			if (SUCCEEDED(pComp->QueryInterface(IID_ITfSource, (void**)&pSource))) {
				pSource->UnadviseSink(*pdwCookie);
				pSource->Release();
			}
			pComp->Release();
		}
		pCompMgr->Release();
	}
	*pdwCookie = TF_INVALID_COOKIE;
}

void ChmTsfInterface::_SyncImeOpenCloseFromCompartment(ITfContext* pic)
{
	_SetImeOpenState(_GetCompartmentBool(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, FALSE), pic, FALSE);
}

void ChmTsfInterface::_ApplyAppInputMode(ITfContext* pic)
{
	if (_GetCompartmentBool(GUID_COMPARTMENT_KEYBOARD_DISABLED, FALSE) ||
		_GetCompartmentBool(GUID_COMPARTMENT_EMPTYCONTEXT, FALSE)) {
		ChmLogger::Info(L"Keyboard disabled/empty context detected; Hitomoji IME forced OFF");
		_SetImeOpenState(FALSE, nullptr, FALSE);
		return;
	}

	if (_IsPasswordContext(pic)) {
		ChmLogger::Info(L"Password input scope detected; Hitomoji IME forced OFF");
		_SetImeOpenState(FALSE, nullptr, TRUE);
		return;
	}

	_SyncImeOpenCloseFromCompartment(pic);
}

BOOL ChmTsfInterface::_GetCompartmentBool(REFGUID rguid, BOOL defaultValue)
{
	if (!_pThreadMgr) return defaultValue;

	ITfCompartmentMgr* pCompMgr = nullptr;
	if (FAILED(_pThreadMgr->QueryInterface(IID_ITfCompartmentMgr, (void**)&pCompMgr))) {
		return defaultValue;
	}

	BOOL value = defaultValue;
	ITfCompartment* pComp = nullptr;
	if (SUCCEEDED(pCompMgr->GetCompartment(rguid, &pComp))) {
		VARIANT var;
		VariantInit(&var);
		if (SUCCEEDED(pComp->GetValue(&var))) {
			if (var.vt == VT_I4) {
				value = (var.lVal != 0);
			} else if (var.vt == VT_BOOL) {
				value = (var.boolVal != VARIANT_FALSE);
			}
			VariantClear(&var);
		}
		pComp->Release();
	}
	pCompMgr->Release();
	return value;
}

HRESULT ChmTsfInterface::_GetFocusedContext(ITfContext** ppContext)
{
	if (!ppContext) return E_INVALIDARG;
	*ppContext = nullptr;
	if (!_pThreadMgr) return E_FAIL;

	ITfDocumentMgr* pDocMgr = nullptr;
	HRESULT hr = _pThreadMgr->GetFocus(&pDocMgr);
	if (FAILED(hr) || !pDocMgr) return hr;

	hr = _GetTopContext(pDocMgr, ppContext);
	pDocMgr->Release();
	return hr;
}

HRESULT ChmTsfInterface::_GetTopContext(ITfDocumentMgr* pDocMgr, ITfContext** ppContext)
{
	if (!ppContext) return E_INVALIDARG;
	*ppContext = nullptr;
	if (!pDocMgr) return E_INVALIDARG;

	return pDocMgr->GetTop(ppContext);
}

BOOL ChmTsfInterface::_IsPasswordContext(ITfContext* pic)
{
	if (!pic) return FALSE;

	ITfInputScope* pInputScope = nullptr;
	if (FAILED(pic->QueryInterface(IID_ITfInputScope, (void**)&pInputScope)) || !pInputScope) {
		return FALSE;
	}

	InputScope* pScopes = nullptr;
	UINT count = 0;
	BOOL fPassword = FALSE;
	if (SUCCEEDED(pInputScope->GetInputScopes(&pScopes, &count)) && pScopes) {
		for (UINT i = 0; i < count; ++i) {
			if (pScopes[i] == IS_PASSWORD) {
				fPassword = TRUE;
				break;
			}
		}
		CoTaskMemFree(pScopes);
	}
	pInputScope->Release();
	return fPassword;
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
