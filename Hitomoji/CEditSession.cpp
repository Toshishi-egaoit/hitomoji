#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msctf.h>
#include <string>

#include "CEditSession.h"
#include "TsfIf.h"
#include "DisplayAttribute.h"

CEditSessionBase::CEditSessionBase(
	ITfContext* pic,
	TfClientId tid,
	ChmTsfInterface* pTsfIf)
	: _pic(pic), _tid(tid), _pTsfIf(pTsfIf), _cRef(1) {
}

STDMETHODIMP CEditSessionBase::QueryInterface(REFIID riid, void** ppv) {
	if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
		*ppv = this; AddRef(); return S_OK;
	}
	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CEditSessionBase::AddRef() {
	return ++_cRef;
}

STDMETHODIMP_(ULONG) CEditSessionBase::Release() {
	if (--_cRef == 0) {
		delete this;
		return 0;
	}
	return _cRef;
}

STDMETHODIMP CEditSessionBase::OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* pComposition) {
	return S_OK;
}

CEditSessionBase::~CEditSessionBase() {
}

HRESULT CEditSessionBase::_StartComposition(TfEditCookie ec, ITfRange* pRange) {
	if (!pRange) return E_INVALIDARG;

	ITfContextComposition* pCtxComp = nullptr;
	HRESULT hr = _pic->QueryInterface(IID_ITfContextComposition, (void**)&pCtxComp);
	if (FAILED(hr) || !pCtxComp) {
		return E_FAIL;
	}

	ITfComposition* pNewComp = nullptr;
	hr = pCtxComp->StartComposition(ec, pRange, this, &pNewComp);
	pCtxComp->Release();

	if (FAILED(hr) || !pNewComp) {
		return E_FAIL;
	}

	_pTsfIf->SetComposition(_pic, pNewComp);
	return S_OK;
}

HRESULT CEditSessionBase::_GetOrStartComposition(TfEditCookie ec, ITfRange** ppRange) {
	if (!ppRange) return E_INVALIDARG;
	*ppRange = nullptr;

	ITfComposition* pComp = _pTsfIf->GetComposition();
	if (pComp) {
		HRESULT hr = pComp->GetRange(ppRange);
		if (SUCCEEDED(hr) && *ppRange) {
			return S_OK;
		}
	}

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

HRESULT CEditSessionBase::_ApplyDisplayAttribute(TfEditCookie ec, ITfRange* pRange) {
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

void CEditSessionBase::_MoveCaretToRangeEnd(TfEditCookie ec, ITfRange* pRange) {
	pRange->Collapse(ec, TF_ANCHOR_END);

	TF_SELECTION sel;
	sel.range = pRange;
	sel.style.ase = TF_AE_NONE;
	sel.style.fInterimChar = FALSE;
	_pic->SetSelection(ec, 1, &sel);
}

void CEditSessionBase::_TerminateComposition(TfEditCookie ec) {
	ITfComposition* pComp = _pTsfIf->GetComposition();
	if (!pComp) return;
	pComp->EndComposition(ec);
	_pTsfIf->ClearComposition();
}

CEditSession::CEditSession(
	ITfContext* pic,
	TfClientId tid,
	ChmTsfInterface* pTsfIf,
	std::wstring compositionStr,
	BOOL fEnd)
	: CEditSessionBase(pic, tid, pTsfIf), _compositionStr(compositionStr), _fEnd(fEnd) {
}

STDMETHODIMP CEditSession::DoEditSession(TfEditCookie ec) {
	OutputDebugStringWithString(L"[hitomoji] DoEditSession:>>%s<<", _compositionStr.c_str());
	ITfRange* pRange = nullptr;

	if (FAILED(_GetOrStartComposition(ec, &pRange))) return S_OK;

	pRange->SetText(ec, TF_ST_CORRECTION, _compositionStr.c_str(), (LONG)_compositionStr.length());

	if (_fEnd) {
		_MoveCaretToRangeEnd(ec, pRange);
		_TerminateComposition(ec);
	} else {
		LONG cch = (LONG)_compositionStr.length();
		LONG shifted = 0;
		pRange->ShiftEnd(ec, cch, &shifted, nullptr);
		_ApplyDisplayAttribute(ec, pRange);
	}

	if (pRange) pRange->Release();
	_pTsfIf->SetMyEditSessionTick();
	return S_OK;
}

CUndoEditSession::CUndoEditSession(
	ITfContext* pic,
	TfClientId tid,
	ChmTsfInterface* pTsfIf,
	std::wstring compositionStr,
	LONG deleteLength)
	: CEditSessionBase(pic, tid, pTsfIf), _compositionStr(compositionStr), _deleteLength(deleteLength) {
}

STDMETHODIMP CUndoEditSession::DoEditSession(TfEditCookie ec) {
	OutputDebugStringWithString(L"[hitomoji] UndoEditSession:>>%s<<", _compositionStr.c_str());

	ITfRange* pRange = nullptr;
	if (FAILED(_GetDeleteRange(ec, &pRange))) return S_OK;

	HRESULT hr = pRange->SetText(ec, TF_ST_CORRECTION, L"", 0);
	if (FAILED(hr)) {
		pRange->Release();
		return S_OK;
	}

	hr = _StartComposition(ec, pRange);
	if (FAILED(hr)) {
		pRange->Release();
		return S_OK;
	}

	pRange->SetText(ec, TF_ST_CORRECTION, _compositionStr.c_str(), (LONG)_compositionStr.length());
	LONG cch = (LONG)_compositionStr.length();
	LONG shifted = 0;
	pRange->ShiftEnd(ec, cch, &shifted, nullptr);
	_ApplyDisplayAttribute(ec, pRange);

	pRange->Release();
	_pTsfIf->SetMyEditSessionTick();
	return S_OK;
}

HRESULT CUndoEditSession::_GetDeleteRange(TfEditCookie ec, ITfRange** ppRange) {
	if (!ppRange) return E_INVALIDARG;
	*ppRange = nullptr;
	if (_deleteLength <= 0) return E_INVALIDARG;

	TF_SELECTION sel;
	ULONG fetched = 0;
	HRESULT hr = _pic->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched);
	if (FAILED(hr) || fetched != 1 || !sel.range) {
		return E_FAIL;
	}

	ITfRange* pRange = sel.range;
	pRange->Collapse(ec, TF_ANCHOR_START);

	LONG shifted = 0;
	hr = pRange->ShiftStart(ec, -_deleteLength, &shifted, nullptr);
	if (FAILED(hr) || shifted != -_deleteLength) {
		pRange->Release();
		return E_FAIL;
	}

	*ppRange = pRange;
	return S_OK;
}
