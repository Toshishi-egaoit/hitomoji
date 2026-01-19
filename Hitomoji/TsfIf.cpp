// TsfIf.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msctf.h>
#include <cstdio>
#include <string>
#include <cctype>
#include <initguid.h> // GUIDの実体定義用
#include "TsfIf.h"
#include "DisplayAttribute.h"
#include <objbase.h>

class CEditSession : public ITfEditSession , public ITfCompositionSink{

public:
	CEditSession(
		ITfContext* pic, 
		ITfComposition** ppComp, 
		TfClientId tid, 
		std::wstring compositionStr,
		BOOL fEnd)
		: _pic(pic), _ppComp(ppComp), _tid(tid), _compositionStr(compositionStr), _fEnd(fEnd), _cRef(1) {
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

	// 現状で使用している実用版のDoEditSession（ある程度の構造化をしてある）
	STDMETHODIMP DoEditSession(TfEditCookie ec) {
		OutputDebugStringWithString(L"[hitomoji] DoEditSession:>>%s<<",_compositionStr.c_str());
		ITfRange* pRange = nullptr;

		// 1. Compositionの準備
		if (FAILED(_GetOrStartComposition(ec, &pRange))) return S_OK;

		// 2. 確定処理
		if (_fEnd ) {
			pRange->SetText(ec, TF_ST_CORRECTION, _compositionStr.c_str(), (LONG)_compositionStr.length());
			_TerminateComposition(ec);
		} else {
			// 3. 変換中文字列の表示
			LONG temp = 0;
			pRange->SetText(ec, TF_ST_CORRECTION, _compositionStr.c_str(), (LONG)_compositionStr.length());
			pRange->ShiftStart(ec, 0, &temp, nullptr); // Composition全体を選択状態に
			_ApplyDisplayAttribute(ec, pRange);
		}

		if (pRange) pRange->Release();
		return S_OK;
	}

private:
	// 既存のCompositionを取得するか、なければ新しく開始する（改良版）
	HRESULT _GetOrStartComposition(TfEditCookie ec, ITfRange** ppRange) {
		if (*_ppComp == nullptr) { // 新規Composition開始
			ITfInsertAtSelection* pInsert;
			if (SUCCEEDED(_pic->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsert))) {
				HRESULT hr = pInsert->InsertTextAtSelection(ec, TS_IAS_QUERYONLY, L"", 0, ppRange);
				OUTPUT_HR_n_RETURN_ON_ERROR(L"InsertTextAtSelection",hr);

				ITfContextComposition* pCtxComp = nullptr;
				if (SUCCEEDED(_pic->QueryInterface(IID_ITfContextComposition, (void**)&pCtxComp))) {
					hr = pCtxComp->StartComposition(ec, *ppRange, this, _ppComp);
					OUTPUT_HR_n_RETURN_ON_ERROR(L"StartComposition",hr);
					pCtxComp->Release(); // 目的の_ppCompは取得したので、ReleaseしてOK
				}
				pInsert->Release();
			}
		}
	
		if (*_ppComp != nullptr) {
			(*_ppComp)->GetRange(ppRange);
			return S_OK;
		}
		return E_FAIL;
	}

    // 下線を引くための詳細実装を切り出す
    HRESULT _ApplyDisplayAttribute(TfEditCookie ec, ITfRange* pRange) {
		OutputDebugString(L"[hitomoji] _ApplyDisplayAttribute");
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

    void _TerminateComposition(TfEditCookie ec) {
        if (*_ppComp) {
            (*_ppComp)->EndComposition(ec);
            (*_ppComp)->Release();
            *_ppComp = nullptr;
        }
    }
    // メンバ変数
    ITfContext* _pic; ITfComposition** _ppComp; TfClientId _tid; 
	std::wstring _compositionStr;
	BOOL _fEnd;

	LONG _cRef;
};

// --- CHitomoji クラスの実装 ---

ChmTsfInterface::ChmTsfInterface() : _tfClientId(0), _cRef(1), _pThreadMgr(nullptr), _pComposition(nullptr) {
	_pEngine = new ChmEngine();
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
	} else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
		*ppvObj = (ITfKeyEventSink*)this;
	} else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider)) {
		*ppvObj = (ITfDisplayAttributeProvider*)this;
	}
	if (*ppvObj) { AddRef(); return S_OK; }
	return E_NOINTERFACE;
}
STDMETHODIMP_(ULONG) ChmTsfInterface::AddRef() { return ++_cRef; }
STDMETHODIMP_(ULONG) ChmTsfInterface::Release() {
	if (--_cRef == 0) { delete this; return 0; }
	return _cRef;
}

// ITfTextInputProcessor
STDMETHODIMP ChmTsfInterface::Activate(ITfThreadMgr* ptm, TfClientId tid) {
	_pThreadMgr = ptm;
	_pThreadMgr->AddRef();
	_tfClientId = tid;

	if (FAILED(_InitKeyEventSink())) return E_FAIL;
	if (FAILED(_InitPreservedKey())) return E_FAIL;
	if (FAILED(_InitDisplayAttributeInfo())) return E_FAIL;

	return S_OK;
}

STDMETHODIMP ChmTsfInterface::Deactivate() {
	_UninitKeyEventSink();
	_UninitPreservedKey();
	if (_pThreadMgr) {
		_pThreadMgr->Release();
		_pThreadMgr = nullptr;
	}
	return S_OK;
}

// ITfKeyEventSink Implementation
STDMETHODIMP ChmTsfInterface::OnTestKeyDown(ITfContext* pic, WPARAM wp, LPARAM lp, BOOL* pfEaten) {
	*pfEaten = _pEngine->IsKeyEaten(wp);
	return S_OK;
}

STDMETHODIMP ChmTsfInterface::OnKeyDown(ITfContext* pic, WPARAM wp, LPARAM lp, BOOL* pfEaten)
{
	*pfEaten = _pEngine->IsKeyEaten(wp);
    if (*pfEaten) {
        ChmKeyEvent kEv(wp, lp);
		_pEngine->UpdateComposition(kEv);
		_InvokeEditSession(pic, kEv.ShouldEndComposition());
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
		_pEngine->ToggleIME();
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
		OutputDebugStringWithInt(L"[Hitomoji] InitPreservedKey:", hr);
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

void ChmTsfInterface::_UninitDisplayAttributeInfo() { return ; }

HRESULT ChmTsfInterface::_InvokeEditSession(ITfContext* pic, BOOL fEnd) {
	CEditSession* pES = new CEditSession(pic, &_pComposition, _tfClientId, _pEngine->GetCompositionStr(), fEnd);
	if (!pES) return E_OUTOFMEMORY;
	HRESULT hr;
	HRESULT hrSession = pic->RequestEditSession(_tfClientId, pES, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
	pES->Release();
	return hrSession;
}
// ------

// ITfDisplayAttributeProvider
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
// ------
