#pragma once
#include <windows.h>
#include <msctf.h>
#include <initguid.h>

// --- GUID Definitions ---
// {86B8A5A1-9B5A-4B6E-A77E-6A2F2B1F7B12}
DEFINE_GUID(CLSID_Hitomoji, 0x86b8a5a1, 0x9b5a, 0x4b6e, 0xa7, 0x7e, 0x6a, 0x2f, 0x2b, 0x1f, 0x7b, 0x12);
// {B1A5D3E2-4C1F-4B9A-9D8C-123456789ABC}
DEFINE_GUID(GUID_HmProfile, 0xb1a5d3e2, 0x4c1f, 0x4b9a, 0x9d, 0x8c, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc);
// {E2E1D4C3-5D2E-4C1F-8E9D-AABBCCDDEEFF}
DEFINE_GUID(GUID_HmPreservedKey_OnOff, 0xe2e1d4c3, 0x5d2e, 0x4c1f, 0x8e, 0x9d, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff);

extern HINSTANCE g_hInst;

class CHitomoji : public ITfTextInputProcessor, public ITfKeyEventSink {
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
    STDMETHODIMP OnSetFocus(BOOL fForeground);
    STDMETHODIMP OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten);
    STDMETHODIMP OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);

private:
    long _cRef;
    ITfThreadMgr* _ptm;
    TfClientId _tid;
    BOOL _isIMEOn;

    HRESULT _HmInitPreservedKey();
    void _HmUninitPreservedKey();
};