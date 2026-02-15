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
	STDMETHODIMP_(ULONG) AddRef() { 
        InterlockedIncrement(&g_cRefDll);
		return InterlockedIncrement(&_cRef) ;
	}
	STDMETHODIMP_(ULONG) Release() {
        InterlockedDecrement(&g_cRefDll);
		LONG tmp = InterlockedDecrement(&_cRef) ;
		if ( tmp == 0) {
			delete this; 
			return 0; 
		} 
		return tmp;
	}

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
private:
	LONG _cRef = 1;
};

// --- DLLエクスポート関数 ---

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID pvReserved) {

    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hInst = hInst;
        DisableThreadLibraryCalls(hInst);
		OutputDebugString(L"[Hitomoji] dllmain(PROCESS_ATTACH");
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
	OutputDebugStringWithGuid(L"[Hitomoji]DllGetClassObject:%s",riid);
    if (IsEqualCLSID(rclsid, CLSID_Hitomoji)) {
        CHitomojiClassFactory* pCF = new CHitomojiClassFactory();
        if (!pCF) return E_OUTOFMEMORY;
        HRESULT hr = pCF->QueryInterface(riid, ppv);
		OUTPUT_HR_ON_ERROR(L"pCF->QueryInterface",hr)
		pCF->Release();
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
