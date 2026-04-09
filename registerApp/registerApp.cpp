// RegisterApp.cpp
#define INITGUID
#include <initguid.h>

// RegisterApp.cpp
#include <windows.h>
#include <msctf.h>
#include <combaseapi.h>
#include <string>
#include "TsfIf.h"
#include "hitomoji.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

int main() {
	wprintf(L"register hitomoji for v" HM_VERSION 
#ifdef _WIN64
		L" (x64)"
#else
		L" (x86)"
#endif
		L"\n");
    (void)CoInitialize(NULL);

    // 自分のDLLのフルパスを取得（RegisterApp.exeと同じフォルダにある想定）
    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(NULL, dllPath, MAX_PATH);
    std::wstring pathStr = dllPath;
    pathStr = pathStr.substr(0, pathStr.find_last_of(L"\\")) + L"\\Hitomoji.dll";

    // 1. COMの登録 (regsvr32の代わり)
	// このしょりは、DLL側のDllRegisterServerにいどう

    // 2. TSFの登録
    ITfInputProcessorProfiles* pProfiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (void**)&pProfiles);
    if (SUCCEEDED(hr)) {
        hr = pProfiles->Register(CLSID_Hitomoji);
        if (SUCCEEDED(hr)) {
            hr = pProfiles->AddLanguageProfile(CLSID_Hitomoji, 0x0411, GUID_HmProfile, 
				L"ひともじ v" HM_VERSION L"(" __DATE__ L" " __TIME__ L")", 4, pathStr.c_str(), (ULONG)pathStr.length(), 0);
            
            ITfCategoryMgr* pCategoryMgr = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (void**)&pCategoryMgr))) {
                // キーボードとして登録
                pCategoryMgr->RegisterCategory(CLSID_Hitomoji, GUID_TFCAT_TIP_KEYBOARD, CLSID_Hitomoji);
                // ★Windows 11で必須：モダンアプリ・設定画面対応
                pCategoryMgr->RegisterCategory(CLSID_Hitomoji, GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT, CLSID_Hitomoji);
                pCategoryMgr->Release();
            }
            printf("  registered 'Hitomoji'\n");
        }
        pProfiles->Release();
    }
    
    CoUninitialize();
    return 0;
}
