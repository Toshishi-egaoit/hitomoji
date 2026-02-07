// TsfIf.h
#pragma once
#include <msctf.h>
#include "utils.h"
#include "ChmEngine.h"

#define CHM_ONENDEDIT_TICK 30  // OnEndEdit セッションとみなす時間間隔（ms）

extern HINSTANCE g_hInst;

// --- GUID Definitions ---
// {86B8A5A1-9B5A-4B6E-A77E-6A2F2B1F7B12}
DEFINE_GUID(CLSID_Hitomoji, 0x86b8a5a1, 0x9b5a, 0x4b6e, 0xa7, 0x7e, 0x6a, 0x2f, 0x2b, 0x1f, 0x7b, 0x12);
// {B1A5D3E2-4C1F-4B9A-9D8C-123456789ABC}
DEFINE_GUID(GUID_HmProfile, 0xb1a5d3e2, 0x4c1f, 0x4b9a, 0x9d, 0x8c, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc);

// --- 標準的なGUIDの定義 ---
// {5AD346D8-9B21-460b-AF53-0604AD7A94D4}
//DEFINE_GUID(GUID_TFCAT_DISPLAYATTRIBUTE_TRACKING, 0x5ad346d8, 0x9b21, 0x460b, 0xaf, 0x53, 0x06, 0x04, 0xad, 0x7a, 0x94, 0xd4);
// {60a54235-d053-4b61-9c59-55648c997120}
// DEFINE_GUID(GUID_PROP_ATTRIBUTE, 0x60a54235, 0xd053, 0x4b61, 0x9c, 0x59, 0x55, 0x64, 0x8c, 0x99, 0x71, 0x20);
    
// Ctrl + Space の定義
const TF_PRESERVEDKEY c_presKeyOpenClose = { VK_SPACE, TF_MOD_CONTROL };
const GUID GUID_PreservedKey_OpenClose = { 0x6e9f9050, 0x05f8, 0x479c, { 0x82, 0x6e, 0x16, 0x93, 0x4e, 0x62, 0xe1, 0x01 } };

class ChmTsfInterface : public ITfTextInputProcessor,
		public ITfKeyEventSink ,
		public ITfDisplayAttributeProvider,
		public ITfThreadFocusSink,
		public ITfTextEditSink{
public:
    ChmTsfInterface();
    ~ChmTsfInterface();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // ITfTextInputProcessor
    STDMETHODIMP Activate(ITfThreadMgr* ptm, TfClientId tid);
    STDMETHODIMP Deactivate();

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL fForeground) ;
    STDMETHODIMP OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten);

	// ITfDisplayAttributeProvider
	STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo **ppInfo) ;
	STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo **ppEnum);

	// ITfThreadFocusSink
	STDMETHODIMP OnSetThreadFocus() ;
	STDMETHODIMP OnKillThreadFocus();

	// ITfTextEditSink
	STDMETHODIMP OnEndEdit(ITfContext* pic, TfEditCookie ecReadOnly, ITfEditRecord* pEditRecord) ;

	// Composition管理

	void SetMyEditSessionTick() { _llMyEditSessionTick = GetTickCount64() ; }
	BOOL IsMyEditSession() { return (GetTickCount64() - _llMyEditSessionTick <= CHM_ONENDEDIT_TICK) ; }
	ITfComposition* GetComposition() const { return _pComposition; }
	ITfContext* GetCompositionContext() const { return _pContextForComposition; }
	void SetComposition(ITfContext* pic, ITfComposition* pComp)
	{
		ClearComposition();
		// ITfContextComposition::StartComposition で AddRef されている前提
		_pComposition = pComp;
		_pContextForComposition = pic;
		if (_pContextForComposition) {
			_pContextForComposition->AddRef();
		}
	}

	void ClearComposition()
	{
		if (_pComposition)
		{
			OutputDebugString(L"ClearComposition");
			_pComposition->Release();
			_pComposition = nullptr;
		}
		if (_pContextForComposition)
		{
			_pContextForComposition->Release();
			_pContextForComposition = nullptr;
		}
	}

	// Composition管理
	// 観測専用：Composition があれば最初の View を返す
	// 戻り値：S_OK + *ppView != nullptr → Composition あり
	//         S_FALSE                    → Composition なし
	//         それ以外                   → エラー
	HRESULT GetFirstCompositionView(
		ITfContext* pic,
		ITfCompositionView** ppView
	);

private:

    // ITfKeyEventSink
    HRESULT _InitKeyEventSink();
    void _UninitKeyEventSink();
    HRESULT _InitPreservedKey();
    void _UninitPreservedKey();
	HRESULT _InvokeEditSession(ITfContext* pic, BOOL fEnd) ;

	// ITfDisplayAttributeProvider
    HRESULT _InitDisplayAttributeInfo();
    void _UninitDisplayAttributeInfo();
    
	// ITfTextEditSink
	HRESULT _InitTextEditSink(ITfContext* pic);
	HRESULT _UninitTextEditSink(ITfContext* pic);

    ITfThreadMgr* _pThreadMgr;
    TfClientId _tfClientId;
    LONG _cRef;

    DWORD _dwThreadFocusSinkCookie; 
    DWORD _dwTextEditSinkCookie; 
	LONGLONG _llMyEditSessionTick;
    ITfComposition* _pComposition;
	ITfContext* _pContextForComposition;
    ChmEngine* _pEngine; // ロジック担当
};

#define OUTPUT_HR(funcName,hr) \
{\
	WCHAR buf[128];\
	swprintf_s(buf, funcName L" failed HR: 0x%08X", hr);\
	OutputDebugString(buf);\
}

#define OUTPUT_HR_ON_ERROR(funcName,hr) \
if ( hr != S_OK ) {\
	OUTPUT_HR(funcName,hr)\
};

#define OUTPUT_HR_n_RETURN_ON_ERROR(funcName,hr) \
if ( hr != S_OK ) {\
	OUTPUT_HR(funcName,hr)\
	return hr;\
};

