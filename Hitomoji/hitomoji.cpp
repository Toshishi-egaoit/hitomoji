#include "Hitomoji.h"

// リンカーへのエクスポート指示（.defファイルの代わり）
#pragma comment(linker, "/export:DllGetClassObject,PRIVATE")
#pragma comment(linker, "/export:DllCanUnloadNow,PRIVATE")
#pragma comment(linker, "/export:DllRegisterServer,PRIVATE")
#pragma comment(linker, "/export:DllUnregisterServer,PRIVATE")

HINSTANCE g_hInst = NULL;

// --- DLL Exports (実体) ---

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID pvReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hInst = hInst;
        // DLL読み込み時にログを出す（生存確認用）
        OutputDebugStringW(L"[Hitomoji] DllMain: Process Attach\n");
    }
    return TRUE;
}

// COMクラスファクトリの生成
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!IsEqualCLSID(rclsid, CLSID_Hitomoji)) return CLASS_E_CLASSNOTAVAILABLE;
    
    CHitomoji* pIME = new CHitomoji();
    if (!pIME) return E_OUTOFMEMORY;

    HRESULT hr = pIME->QueryInterface(riid, ppv);
    pIME->Release();
    return hr;
}

// DLLをアンロードして良いかシステムが確認する関数
STDAPI DllCanUnloadNow() {
    return S_OK; 
}

// regsvr32 実行時に呼ばれる。今回はRegisterAppで登録するため成功(S_OK)を返すだけでOK
STDAPI DllRegisterServer() {
    return S_OK;
}

// regsvr32 /u 実行時に呼ばれる
STDAPI DllUnregisterServer() {
    return S_OK;
}

// --- CHitomoji Implementation ---
// (※以下、前回の実装と同じですが、一貫性のために主要部分を記載)

CHitomoji::CHitomoji() : _cRef(1), _ptm(nullptr), _tid(0), _isIMEOn(FALSE) {}
CHitomoji::~CHitomoji() {}

STDMETHODIMP CHitomoji::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor)) 
        *ppvObj = (ITfTextInputProcessor*)this;
    else if (IsEqualIID(riid, IID_ITfKeyEventSink)) 
        *ppvObj = (ITfKeyEventSink*)this;
    
    if (*ppvObj) { AddRef(); return S_OK; }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CHitomoji::AddRef() { return InterlockedIncrement(&_cRef); }
STDMETHODIMP_(ULONG) CHitomoji::Release() {
    long c = InterlockedDecrement(&_cRef);
    if (c == 0) delete this;
    return c;
}

STDMETHODIMP CHitomoji::Activate(ITfThreadMgr* ptm, TfClientId tid) {
    OutputDebugStringW(L"[Hitomoji] Activate Called\n");
    _ptm = ptm; _ptm->AddRef();
    _tid = tid;

    ITfSource* pSource = nullptr;
    if (ptm->QueryInterface(IID_ITfSource, (void**)&pSource) == S_OK) {
        pSource->AdviseSink(IID_ITfKeyEventSink, (ITfKeyEventSink*)this, &tid);
        pSource->Release();
    }
    return _HmInitPreservedKey();
}

STDMETHODIMP CHitomoji::Deactivate() {
    OutputDebugStringW(L"[Hitomoji] Deactivate Called\n");
    _HmUninitPreservedKey();
    if (_ptm) { _ptm->Release(); _ptm = nullptr; }
    return S_OK;
}

HRESULT CHitomoji::_HmInitPreservedKey() {
    ITfKeystrokeMgr* pksm = nullptr;
    if (_ptm->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pksm) == S_OK) {
        TF_PRESERVEDKEY pk = { VK_KANJI, 0 }; 
        pksm->PreserveKey(_tid, GUID_HmPreservedKey_OnOff, &pk, L"ひともじ ON/OFF", 8);
        pksm->Release();
    }
    return S_OK;
}

void CHitomoji::_HmUninitPreservedKey() {
    ITfKeystrokeMgr* pksm = nullptr;
    if (_ptm && _ptm->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pksm) == S_OK) {
        TF_PRESERVEDKEY pk = { VK_KANJI, 0 };
        pksm->UnpreserveKey(GUID_HmPreservedKey_OnOff, &pk);
        pksm->Release();
    }
}

STDMETHODIMP CHitomoji::OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten) {
    if (IsEqualGUID(rguid, GUID_HmPreservedKey_OnOff)) {
        _isIMEOn = !_isIMEOn;
        OutputDebugStringW(_isIMEOn ? L"[Hitomoji] State: ON\n" : L"[Hitomoji] State: OFF\n");
        *pfEaten = TRUE;
    } else { *pfEaten = FALSE; }
    return S_OK;
}

// 以下のメソッドもパススルー（空実装）として必要
STDMETHODIMP CHitomoji::OnSetFocus(BOOL fForeground) { return S_OK; }
STDMETHODIMP CHitomoji::OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) { *pfEaten = FALSE; return S_OK; }
STDMETHODIMP CHitomoji::OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) { *pfEaten = FALSE; return S_OK; }
STDMETHODIMP CHitomoji::OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) { *pfEaten = FALSE; return S_OK; }
STDMETHODIMP CHitomoji::OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) { *pfEaten = FALSE; return S_OK; }