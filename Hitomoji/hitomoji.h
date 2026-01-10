// Hitomoji.h
#pragma once
#include <msctf.h>
#include "Controller.h"

// --- GUID Definitions ---
// {86B8A5A1-9B5A-4B6E-A77E-6A2F2B1F7B12}
DEFINE_GUID(CLSID_Hitomoji, 0x86b8a5a1, 0x9b5a, 0x4b6e, 0xa7, 0x7e, 0x6a, 0x2f, 0x2b, 0x1f, 0x7b, 0x12);
// {B1A5D3E2-4C1F-4B9A-9D8C-123456789ABC}
DEFINE_GUID(GUID_HmProfile, 0xb1a5d3e2, 0x4c1f, 0x4b9a, 0x9d, 0x8c, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc);

// --- 標準的なGUIDの定義 ---
// {5AD346D8-9B21-460b-AF53-0604AD7A94D4}
DEFINE_GUID(GUID_TFCAT_DISPLAYATTRIBUTE_TRACKING, 0x5ad346d8, 0x9b21, 0x460b, 0xaf, 0x53, 0x06, 0x04, 0xad, 0x7a, 0x94, 0xd4);
// {60a54235-d053-4b61-9c59-55648c997120}
// DEFINE_GUID(GUID_PROP_ATTRIBUTE, 0x60a54235, 0xd053, 0x4b61, 0x9c, 0x59, 0x55, 0x64, 0x8c, 0x99, 0x71, 0x20);

// 「入力中（変換前）」の標準スタイル
// {191D0A24-AD9A-411a-9488-A7235677A382}
DEFINE_GUID(GUID_ATTR_INPUT,  0x191d0a24, 0xad9a, 0x411a, 0x94, 0x88, 0xa7, 0x23, 0x56, 0x77, 0xa3, 0x82 );
    
// Ctrl + Space の定義
const TF_PRESERVEDKEY c_presKeyOpenClose = { VK_SPACE, TF_MOD_CONTROL };
const GUID GUID_PreservedKey_OpenClose = { 0x6e9f9050, 0x05f8, 0x479c, { 0x82, 0x6e, 0x16, 0x93, 0x4e, 0x62, 0xe1, 0x01 } };

class CHitomoji : public ITfTextInputProcessor,
                  public ITfDisplayAttributeProvider,
                  public ITfKeyEventSink {
public:
    CHitomoji();
    ~CHitomoji();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // ITfTextInputProcessor
    STDMETHODIMP Activate(ITfThreadMgr* ptm, TfClientId tid);
    STDMETHODIMP Deactivate();

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL fForeground) { return S_OK; }
    STDMETHODIMP OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten);

	// ITfDisplayAttributeProvider
	STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo **ppInfo) ;
	STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo **ppEnum);

private:
    HRESULT _InitKeyEventSink();
    void _UninitKeyEventSink();
    HRESULT _InitPreservedKey();
    void _UninitPreservedKey();
    
    HRESULT _InvokeEditSession(ITfContext* pic, WCHAR ch, BOOL fEnd);

    ITfThreadMgr* _pThreadMgr;
    TfClientId _tfClientId;
    LONG _cRef;

    ITfComposition* _pComposition;
    CController* _pController; // ロジック担当
};

// --- debug support functions
extern void OutputDebugStringWithInt(wchar_t const* format, ULONG lvalue);
extern void OutputDebugStringWithString(wchar_t const* format, wchar_t const* value);

#define OUTPUT_HR_n_RETURN_ON_ERROR(funcName,hr) \
if ( hr != S_OK ) {\
	WCHAR buf[128];\
	swprintf_s(buf, funcName L" failed HR: 0x%08X", hr);\
	OutputDebugString(buf);\
	return hr;\
};
