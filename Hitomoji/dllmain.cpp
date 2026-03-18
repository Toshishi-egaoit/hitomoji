// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include <windows.h>
#include <ole2.h>
#include "TsfIf.h"
#include "Hitomoji.h"
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

// CLSIDの登録関数。64/32ビット両対応
HRESULT RegisterCLSID(const wchar_t* dllPath)
{
    HKEY hKey;
    wchar_t clsidStr[64] = {};

    StringFromGUID2(CLSID_Hitomoji, clsidStr, _countof(clsidStr));
    std::wstring keyPath =
        std::wstring(L"Software\\Classes\\CLSID\\") + clsidStr;

    // アクセスフラグ（ここがキモ）
    REGSAM samDesired = KEY_WRITE;

#if defined(_WIN64)
    samDesired |= KEY_WOW64_64KEY;
#else
    samDesired |= KEY_WOW64_32KEY;
#endif

    // 1. CLSIDキーの作成
    if (RegCreateKeyExW(
            HKEY_LOCAL_MACHINE,
            keyPath.c_str(),
            0, NULL, 0,
            samDesired,
            NULL, &hKey, NULL) != ERROR_SUCCESS)
    {
        return E_FAIL;
    }

    RegSetValueExW(
        hKey, NULL, 0, REG_SZ,
        (const BYTE*)L"Hitomoji IME",
        (DWORD)(wcslen(L"Hitomoji IME") + 1) * sizeof(wchar_t)
    );

    // 2. InprocServer32キーの作成
    HKEY hSubKey;
    if (RegCreateKeyExW(
            hKey,
            L"InprocServer32",
            0, NULL, 0,
            samDesired,
            NULL, &hSubKey, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExW(
            hSubKey, NULL, 0, REG_SZ,
            (const BYTE*)dllPath,
            (DWORD)(wcslen(dllPath) + 1) * sizeof(wchar_t)
        );

        RegSetValueExW(
            hSubKey, L"ThreadingModel", 0, REG_SZ,
            (const BYTE*)L"Apartment",
            (DWORD)(wcslen(L"Apartment") + 1) * sizeof(wchar_t)
        );

        RegCloseKey(hSubKey);
    }

    RegCloseKey(hKey);
    return S_OK;
}

// --- DLLエクスポート関数 ---

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID pvReserved) {

    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hInst = hInst;
        DisableThreadLibraryCalls(hInst);
		OutputDebugString(L"[Hitomoji] dllmain(PROCESS_ATTACH) " HM_VERSION  
#if defined(_WIN64)
			L"(x64)"
#else
			L"(x86)"
#endif
			L" (" __DATE__ L" " __TIME__ L")");
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

STDAPI DllRegisterServer() {
	wchar_t dllPath[MAX_PATH];
	GetModuleFileNameW(g_hInst, dllPath, _countof(dllPath));
	OutputDebugStringWithString(L"DllRegisterServer: %s", dllPath);
	return RegisterCLSID(dllPath);
}

STDAPI DllUnregisterServer() {
    return S_OK;
}
