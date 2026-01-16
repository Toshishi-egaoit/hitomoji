// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include <windows.h>
#include <ole2.h>
#include "TsfIf.h"
#include <cstdio>

// --- グローバル変数 ---
HINSTANCE g_hInst = NULL;
LONG g_cRefDll = 0;

// --- クラスファクトリの実装 ---
class CHitomojiClassFactory : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppv = this; AddRef(); return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return 2; }
    STDMETHODIMP_(ULONG) Release() { return 1; }

    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) {
        if (pUnkOuter) return CLASS_E_NOAGGREGATION;
        ChmTsfInterface* pTip = new ChmTsfInterface();
        if (!pTip) return E_OUTOFMEMORY;
        HRESULT hr = pTip->QueryInterface(riid, ppv);
        pTip->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL fLock) {
        if (fLock) InterlockedIncrement(&g_cRefDll);
        else InterlockedDecrement(&g_cRefDll);
        return S_OK;
    }
};

// --- DLLエクスポート関数 ---

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID pvReserved) {
	OutputDebugString(L"[Hitomoji]dllMain");
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hInst = hInst;
		OutputDebugString(L"   > PROCESS ATTACH");
        DisableThreadLibraryCalls(hInst);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
	OutputDebugString(L"[Hitomoji]DllGetClassObject");
    if (IsEqualCLSID(rclsid, CLSID_Hitomoji)) {
        CHitomojiClassFactory* pCF = new CHitomojiClassFactory();
        if (!pCF) return E_OUTOFMEMORY;
        HRESULT hr = pCF->QueryInterface(riid, ppv);
		OUTPUT_HR_ON_ERROR(L"pCF->QueryInterface",hr)
        return hr;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow() {
    return (g_cRefDll == 0) ? S_OK : S_FALSE;
}

// RegisterAppで登録するため、ここは最小限（成功を返すだけ）にする
STDAPI DllRegisterServer() {
    return S_OK;
}

STDAPI DllUnregisterServer() {
    return S_OK;
}
