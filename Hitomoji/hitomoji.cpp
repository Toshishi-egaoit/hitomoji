// Hitomoji.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msctf.h>
#include <cstdio>
#include <initguid.h> // GUIDの実体定義用
#include "Hitomoji.h"

// --- EditSessionの実装 ---
class CEditSession : public ITfEditSession , public ITfCompositionSink{
public:
	CEditSession(ITfContext* pic, ITfComposition** ppComp, TfClientId tid, WCHAR ch, BOOL fEnd) 
		: _pic(pic), _ppComp(ppComp), _tid(tid), _ch(ch), _fEnd(fEnd), _cRef(1) {}

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

//----- ITfEditSession の実装
	STDMETHODIMP DoEditSession_simple(TfEditCookie ec) {
		ITfRange* pRange = nullptr;
		if (*_ppComp == nullptr) { // 新規Composition開始
			ITfInsertAtSelection* pInsert;
			if (SUCCEEDED(_pic->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsert))) {
				HRESULT hr = pInsert->InsertTextAtSelection(ec, TS_IAS_QUERYONLY, L"", 0, &pRange);
				OUTPUT_HR_n_RETURN_ON_ERROR(L"InsertTextAtSelection",hr);

				ITfContextComposition* pCtxComp = nullptr;
				if (SUCCEEDED(_pic->QueryInterface(IID_ITfContextComposition, (void**)&pCtxComp))) {
					hr = pCtxComp->StartComposition(ec, pRange, this, _ppComp);
					OUTPUT_HR_n_RETURN_ON_ERROR(L"StartComposition",hr);
					pCtxComp->Release(); // 目的の_ppCompは取得したので、ReleaseしてOK
				}
				pInsert->Release();
			}
		}
		else { // 既存Compositionある場合
			(*_ppComp)->GetRange(&pRange);
		}

		if (pRange) {
			if (_fEnd) {
				if (*_ppComp) {
					(*_ppComp)->EndComposition(ec);
					(*_ppComp)->Release();
					*_ppComp = nullptr;
				}
			} else if (_ch != 0) {
				// 文末に文字を追加
				pRange->Collapse(ec, TF_ANCHOR_END);
				pRange->SetText(ec, 0, &_ch, 1);
				_ApplyDisplayAttribute(ec, pRange);
			}
			pRange->Release();
		}
		return S_OK;
	}

	STDMETHODIMP DoEditSession(TfEditCookie ec) {
		OutputDebugString(L"[hitomoji] DoEditSession");
		ITfRange* pRange = nullptr;

		// 1. Compositionの準備
		if (FAILED(_GetOrStartComposition(ec, &pRange))) return S_OK;

		if (_fEnd) {
			// 2. 確定処理
			_TerminateComposition(ec);
		} else if (_ch != 0) {
			// 3. テキストセットと属性付与
			pRange->Collapse(ec, TF_ANCHOR_END);
			pRange->SetText(ec, 0, &_ch, 1);
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
    void _ApplyDisplayAttribute(TfEditCookie ec, ITfRange* pRange) {
		OutputDebugString(L"[hitomoji] _ApplyDisplayAttribute");
        ITfProperty* pProp = nullptr;
		OutputDebugString(L"[hitomoji] GetProperty");
        if (SUCCEEDED(_pic->GetProperty(GUID_PROP_ATTRIBUTE, &pProp))) {
            ITfCategoryMgr* pCategoryMgr = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (void**)&pCategoryMgr))) {
                TfGuidAtom attrAtom = 0;
                if (SUCCEEDED(pCategoryMgr->RegisterGUID(GUID_TFCAT_DISPLAYATTRIBUTE_TRACKING, &attrAtom))) {
                    VARIANT var;
                    var.vt = VT_I4;
                    var.lVal = (LONG)attrAtom;
                    pProp->SetValue(ec, pRange, &var);
                }
                pCategoryMgr->Release();
            }
            pProp->Release();
        }
    }

    void _TerminateComposition(TfEditCookie ec) {
        if (*_ppComp) {
            (*_ppComp)->EndComposition(ec);
            (*_ppComp)->Release();
            *_ppComp = nullptr;
        }
    }
    // メンバ変数はそのまま
    ITfContext* _pic; ITfComposition** _ppComp; TfClientId _tid; WCHAR _ch; BOOL _fEnd; LONG _cRef;
};

// --- CHitomoji クラスの実装 ---

CHitomoji::CHitomoji() : _cRef(1), _pThreadMgr(nullptr), _pComposition(nullptr) {
	_pController = new CController();
}

CHitomoji::~CHitomoji() {
	delete _pController;
}

// IUnknown Implementation
STDMETHODIMP CHitomoji::QueryInterface(REFIID riid, void** ppvObj) {
	if (!ppvObj) return E_INVALIDARG;
	*ppvObj = nullptr;
	if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor)) {
		*ppvObj = (ITfTextInputProcessor*)this;
	} else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
		*ppvObj = (ITfKeyEventSink*)this;
	}
	if (*ppvObj) { AddRef(); return S_OK; }
	return E_NOINTERFACE;
}
STDMETHODIMP_(ULONG) CHitomoji::AddRef() { return ++_cRef; }
STDMETHODIMP_(ULONG) CHitomoji::Release() {
	if (--_cRef == 0) { delete this; return 0; }
	return _cRef;
}

// ITfTextInputProcessor
STDMETHODIMP CHitomoji::Activate(ITfThreadMgr* ptm, TfClientId tid) {
	_pThreadMgr = ptm;
	_pThreadMgr->AddRef();
	_tfClientId = tid;

	if (FAILED(_InitKeyEventSink())) return E_FAIL;
	if (FAILED(_InitPreservedKey())) return E_FAIL;

	return S_OK;
}

STDMETHODIMP CHitomoji::Deactivate() {
	_UninitKeyEventSink();
	_UninitPreservedKey();
	if (_pThreadMgr) {
		_pThreadMgr->Release();
		_pThreadMgr = nullptr;
	}
	return S_OK;
}

// ITfKeyEventSink Implementation
STDMETHODIMP CHitomoji::OnTestKeyDown(ITfContext* pic, WPARAM wp, LPARAM lp, BOOL* pfEaten) {
	*pfEaten = _pController->IsKeyEaten(wp);
	OutputDebugString(L"[Hitomoji]OnTestKeyDown");
	return S_OK;
}

STDMETHODIMP CHitomoji::OnKeyDown(ITfContext* pic, WPARAM wp, LPARAM lp, BOOL* pfEaten) {
	*pfEaten = _pController->IsKeyEaten(wp);
	if (*pfEaten) {
		OutputDebugString(L"[Hitomoji]OnKeyDown:processed");
		if (wp == VK_RETURN) _InvokeEditSession(pic, 0, TRUE);
		else _InvokeEditSession(pic, (WCHAR)wp, FALSE);
	}
	return S_OK;
}

STDMETHODIMP CHitomoji::OnTestKeyUp(ITfContext* pic, WPARAM wp, LPARAM lp, BOOL* pfEaten) {
	*pfEaten = _pController->IsKeyEaten(wp);
	return S_OK;
}

STDMETHODIMP CHitomoji::OnKeyUp(ITfContext* pic, WPARAM wp, LPARAM lp, BOOL* pfEaten) {
	*pfEaten = _pController->IsKeyEaten(wp);
	return S_OK;
}

STDMETHODIMP CHitomoji::OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten) {
	if (IsEqualGUID(rguid, GUID_PreservedKey_OpenClose)) {
		_pController->ToggleIME();
		OutputDebugString(L"[Hitomoji]OnPreservedKey:processed");
		*pfEaten = TRUE;
	}
	return S_OK;
}

HRESULT CHitomoji::_InitKeyEventSink() {
	ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
	HRESULT hr = _pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr);
	if (SUCCEEDED(hr)) {
		hr = pKeystrokeMgr->AdviseKeyEventSink(_tfClientId, (ITfKeyEventSink*)this, TRUE);
		pKeystrokeMgr->Release();
	}
	return hr;
}

void CHitomoji::_UninitKeyEventSink() {
	ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
	if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr))) {
		pKeystrokeMgr->UnadviseKeyEventSink(_tfClientId);
		pKeystrokeMgr->Release();
	}
}

HRESULT CHitomoji::_InitPreservedKey() {
	ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
	HRESULT hr = _pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr);
	if (SUCCEEDED(hr)) {
		hr = pKeystrokeMgr->PreserveKey(_tfClientId, GUID_PreservedKey_OpenClose, &c_presKeyOpenClose, L"ひともじ ON/OFF", (ULONG)wcslen(L"ひともじ ON/OFF"));
		OutputDebugStringWithInt(L"[Hitomoji] InitPreservedKey:", hr);
		pKeystrokeMgr->Release();
	}
	return hr;
}

void CHitomoji::_UninitPreservedKey() {
	ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
	if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr))) {
		pKeystrokeMgr->UnpreserveKey(GUID_PreservedKey_OpenClose, &c_presKeyOpenClose);
		pKeystrokeMgr->Release();
	}
}

HRESULT CHitomoji::_InvokeEditSession(ITfContext* pic, WCHAR ch, BOOL fEnd) {
	CEditSession* pES = new CEditSession(pic, &_pComposition, _tfClientId, ch, fEnd);
	if (!pES) return E_OUTOFMEMORY;
	HRESULT hr;
	HRESULT hrSession = pic->RequestEditSession(_tfClientId, pES, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
	pES->Release();
	return hrSession;
}

// ----- debug support functions
void OutputDebugStringWithInt(wchar_t const* format, ULONG lvalue)
{
    wchar_t buff[255];
    // TODO: assertで255を越えたら落ちるようにする

    wsprintf(buff, format, lvalue);
    OutputDebugStringW(buff);
    return;
}

void OutputDebugStringWithString(wchar_t const* format, wchar_t const* value)
{
    wchar_t buff[255];
    // TODO: assertで255を越えたら落ちるようにする

    wsprintf(buff, format, value);
    OutputDebugStringW(buff);
    return;
}
