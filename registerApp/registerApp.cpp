// RegisterApp.cpp
#define INITGUID
#include <initguid.h>

// RegisterApp.cpp
#include <windows.h>
#include <msctf.h>
#include <combaseapi.h>
#include <string>
#include "TsfIf.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

// --- COM(CLSID)を手動で登録する関数 ---
HRESULT RegisterCLSID(const wchar_t* dllPath) {
    HKEY hKey;
    wchar_t clsidStr[64] = {};

    StringFromGUID2(CLSID_Hitomoji, clsidStr, _countof(clsidStr));
    std::wstring keyPath =
    std::wstring(L"Software\\Classes\\CLSID\\") + clsidStr;    
    
    // 1. CLSIDキーの作成
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) return E_FAIL;
    RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)L"Hitomoji IME", (DWORD)(wcslen(L"Hitomoji IME") + 1) * sizeof(wchar_t));
    
    // 2. InprocServer32キーの作成
    HKEY hSubKey;
    if (RegCreateKeyExW(hKey, L"InprocServer32", 0, NULL, 0, KEY_WRITE, NULL, &hSubKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hSubKey, NULL, 0, REG_SZ, (const BYTE*)dllPath, (DWORD)(wcslen(dllPath) + 1) * sizeof(wchar_t));
        RegSetValueExW(hSubKey, L"ThreadingModel", 0, REG_SZ, (const BYTE*)L"Apartment", (DWORD)(wcslen(L"Apartment") + 1) * sizeof(wchar_t));
        RegCloseKey(hSubKey);
    }
    RegCloseKey(hKey);
    return S_OK;
}

int main() {
    CoInitialize(NULL);

    // 自分のDLLのフルパスを取得（RegisterApp.exeと同じフォルダにある想定）
    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(NULL, dllPath, MAX_PATH);
    std::wstring pathStr = dllPath;
    pathStr = pathStr.substr(0, pathStr.find_last_of(L"\\")) + L"\\Hitomoji.dll";

    // 1. COMの登録 (regsvr32の代わり)
    if (FAILED(RegisterCLSID(pathStr.c_str()))) {
        printf("Failed to register CLSID.\n");
        return 1;
    }

    // 2. TSFの登録
    ITfInputProcessorProfiles* pProfiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (void**)&pProfiles);
    if (SUCCEEDED(hr)) {
        hr = pProfiles->Register(CLSID_Hitomoji);
        if (SUCCEEDED(hr)) {
            hr = pProfiles->AddLanguageProfile(CLSID_Hitomoji, 0x0411, GUID_HmProfile, L"ひともじ", 4, L"ひともじ", 4, 0);
            
            ITfCategoryMgr* pCategoryMgr = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (void**)&pCategoryMgr))) {
                // キーボードとして登録
                pCategoryMgr->RegisterCategory(CLSID_Hitomoji, GUID_TFCAT_TIP_KEYBOARD, CLSID_Hitomoji);
                // ★Windows 11で必須：モダンアプリ・設定画面対応
                pCategoryMgr->RegisterCategory(CLSID_Hitomoji, GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT, CLSID_Hitomoji);
                pCategoryMgr->Release();
            }
            printf("Successfully registered 'Hitomoji'!\n");
        }
        pProfiles->Release();
    }
    
    CoUninitialize();
    return 0;
}
