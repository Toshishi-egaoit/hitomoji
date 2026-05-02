#pragma once

#include <windows.h>
#include <msctf.h>
#include <string>

class ChmTsfInterface;

class CEditSessionBase : public ITfEditSession, public ITfCompositionSink {
public:
	CEditSessionBase(
		ITfContext* pic,
		TfClientId tid,
		ChmTsfInterface* pTsfIf);

	STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
	STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* pComposition);

protected:
	virtual ~CEditSessionBase();

	HRESULT _StartComposition(TfEditCookie ec, ITfRange* pRange);
	HRESULT _GetOrStartComposition(TfEditCookie ec, ITfRange** ppRange);
	HRESULT _ApplyDisplayAttribute(TfEditCookie ec, ITfRange* pRange);
	void _MoveCaretToRangeEnd(TfEditCookie ec, ITfRange* pRange);
	void _TerminateComposition(TfEditCookie ec);

	ITfContext* _pic;
	TfClientId _tid;
	ChmTsfInterface* _pTsfIf;

private:
	LONG _cRef;
};

class CEditSession : public CEditSessionBase {
public:
	CEditSession(
		ITfContext* pic,
		TfClientId tid,
		ChmTsfInterface* pTsfIf,
		std::wstring compositionStr,
		BOOL fEnd);

	STDMETHODIMP DoEditSession(TfEditCookie ec);

private:
	std::wstring _compositionStr;
	BOOL _fEnd;
};

class CUndoEditSession : public CEditSessionBase {
public:
	CUndoEditSession(
		ITfContext* pic,
		TfClientId tid,
		ChmTsfInterface* pTsfIf,
		std::wstring compositionStr,
		LONG deleteLength);

	STDMETHODIMP DoEditSession(TfEditCookie ec);

private:
	HRESULT _GetDeleteRange(TfEditCookie ec, ITfRange** ppRange);

	std::wstring _compositionStr;
	LONG _deleteLength;
};
