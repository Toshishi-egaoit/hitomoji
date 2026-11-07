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
	OUTPUT_HR_ON_ERROR(L"InsertTextAtSelection", hr)
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
	OUTPUT_HR_ON_ERROR(L"StartComposition", hr)

	if (FAILED(hr) || !pNewComp) {
		(*ppRange)->Release();
		*ppRange = nullptr;
		return E_FAIL;
	}

	_pTsfIf->SetComposition(_pic, pNewComp);
	return S_OK;
}

HRESULT CEditSessionBase::_ApplyDisplayAttribute(TfEditCookie ec, ITfRange* pRange, bool provisional) {
	ITfProperty* pProp = nullptr;
	HRESULT hr;
	hr = _pic->GetProperty(GUID_PROP_ATTRIBUTE, &pProp);
	OUTPUT_HR_n_RETURN_ON_ERROR(L"GetProperty", hr);

	VARIANT var;
	VariantInit(&var);
	var.vt = VT_I4;
	var.lVal = (LONG)CDisplayAttributeInfo::GetAtom(provisional);
	hr = pProp->SetValue(ec, pRange, &var);
	pProp->Release();
	OUTPUT_HR_ON_ERROR(L"   > SetValue", hr);
	return hr;
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
	Debug(L"Terminating composition");
	pComp->EndComposition(ec);
	_pTsfIf->ClearComposition();
}

CEditSession::CEditSession(
	ITfContext* pic,
	TfClientId tid,
	ChmTsfInterface* pTsfIf,
	std::wstring compositionStr,
	BOOL fEnd,
	LONG provisionalLength)
	: CEditSessionBase(pic, tid, pTsfIf), _compositionStr(compositionStr), _fEnd(fEnd),
	  _provisionalLength(provisionalLength) {
}

STDMETHODIMP CEditSession::DoEditSession(TfEditCookie ec) {
	Info(Format(L"DoEditSession: end=%d >>%s<<", _fEnd, _compositionStr.c_str()));
	ITfRange* pRange = nullptr;

	if (FAILED(_GetOrStartComposition(ec, &pRange))) return S_OK;

	HRESULT hr = pRange->SetText(ec, TF_ST_CORRECTION, _compositionStr.c_str(), (LONG)_compositionStr.length());
	if (FAILED(hr)) {
		pRange->Release();
		OUTPUT_HR_ON_ERROR(L"   > SetText", hr);
		return hr;
	}

	ITfRange* pCompositionRange = nullptr;
	ITfComposition* pComposition = _pTsfIf->GetComposition();
	if (pComposition && SUCCEEDED(pComposition->GetRange(&pCompositionRange)) && pCompositionRange) {
		pRange->Release();
		pRange = pCompositionRange;
	}

	if (_fEnd) {
		if (_provisionalLength > 0 && !_compositionStr.empty()) {
			ITfProperty* pProp = nullptr;
			if (SUCCEEDED(_pic->GetProperty(GUID_PROP_ATTRIBUTE, &pProp))) {
				pProp->Clear(ec, pRange);
				pProp->Release();
			}
		}
		_MoveCaretToRangeEnd(ec, pRange);
		_TerminateComposition(ec);
	} else {
		LONG cch = (LONG)_compositionStr.length();
		_ApplyDisplayAttribute(ec, pRange);
		if (_provisionalLength > 0 && _provisionalLength <= cch) {
			ITfRange* pPrefix = nullptr;
			if (SUCCEEDED(pRange->Clone(&pPrefix))) {
				LONG prefixShifted = 0;
				if (SUCCEEDED(pPrefix->Collapse(ec, TF_ANCHOR_START))
					&& SUCCEEDED(pPrefix->ShiftEnd(ec, _provisionalLength, &prefixShifted, nullptr))
					&& prefixShifted == _provisionalLength) {
					_ApplyDisplayAttribute(ec, pPrefix, true);
				}
				pPrefix->Release();
			}
		}

		ITfContextView* pContextView = nullptr;
		HRESULT hrView = _pic->GetActiveView(&pContextView);
		if (SUCCEEDED(hrView) && pContextView) {
			RECT rc{};
			BOOL clipped = FALSE;
			HRESULT hrExt = pContextView->GetTextExt(ec, pRange, &rc, &clipped);
			if (SUCCEEDED(hrExt)) {
				_pTsfIf->SetCandidateWindowPosition(rc);
			} else {
				_pTsfIf->ClearCandidateWindowPosition();
			}
			pContextView->Release();
		} else {
			_pTsfIf->ClearCandidateWindowPosition();
		}
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
	Debug(Format(L"UndoEditSession:del=%d str=>>%s<<", _deleteLength, _compositionStr.c_str()));

	ITfRange* pRange = nullptr;
	HRESULT hr = _GetDeleteRange(ec, &pRange);
	if (FAILED(hr)) {
		_pTsfIf->_AbortUnFinish();
		return hr;
	}

	// 確定済みの範囲を Composition にしてから、復元した文字列で置き換える。
	hr = _StartComposition(ec, pRange);
	OUTPUT_HR_ON_ERROR(L"Undo StartComposition", hr);
	if (FAILED(hr)) {
		pRange->Release();
		_pTsfIf->_AbortUnFinish();
		return hr;
	}

	hr = pRange->SetText(ec, TF_ST_CORRECTION, _compositionStr.c_str(), (LONG)_compositionStr.length());
	OUTPUT_HR_ON_ERROR(L"Undo SetText", hr);
	if (FAILED(hr)) {
		pRange->Release();
		_TerminateComposition(ec);
		_pTsfIf->_AbortUnFinish();
		return hr;
	}

	pRange->Release();
	pRange = nullptr;
	ITfComposition* pComposition = _pTsfIf->GetComposition();
	if (pComposition && SUCCEEDED(pComposition->GetRange(&pRange)) && pRange) {
		_ApplyDisplayAttribute(ec, pRange);
		_MoveCaretToRangeEnd(ec, pRange);
		pRange->Release();
	}
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

    // 2. 文書の「絶対的な先頭」から新しい Range を作成
    ITfRange* pFullRange = nullptr;
    if (FAILED(_pic->GetStart(ec, &pFullRange))) {
		Debug(L"GetStart failed");
        sel.range->Release();
        return E_FAIL;
    }

    // 3. この pFullRange の終端(End)を、今のカーソル位置にぶつける
    // これにより、pFullRange は「文書の最初から今の位置まで」を完全にカバーする
    BOOL isCaret = FALSE;
    hr = sel.range->IsEmpty(ec, &isCaret);
    if (SUCCEEDED(hr) && isCaret) {
        hr = pFullRange->ShiftEndToRange(ec, sel.range, TF_ANCHOR_END);
    }
    sel.range->Release();

    // 4. End 位置で Collapse する（この時点では見た目上 GetSelection と同じ位置）
    if (FAILED(hr) || !isCaret || FAILED(pFullRange->Collapse(ec, TF_ANCHOR_END))) {
        pFullRange->Release();
        return E_FAIL;
    }

    // 5. 左へ ShiftStart
    // 起点から繋がっている Range なので、壁を越えて左へ ShiftStart できる可能性が高い
    LONG shifted = 0;
    hr = pFullRange->ShiftStart(ec, -_deleteLength, &shifted, nullptr);
    Debug(Format(L"Absolute Range ShiftStart: %d / Required: %d / HRESULT: 0x%08lX",
        shifted, -_deleteLength, static_cast<unsigned long>(hr)));

    if (FAILED(hr) || shifted != -_deleteLength) {
        pFullRange->Release();
        _pTsfIf->_NotifyInputError(_pic);
        return E_FAIL;
    }

    *ppRange = pFullRange;
    return S_OK;
}
