// Minimal TSF-enabled Win32 application with a single-line edit box
#include <windows.h>
#include <msctf.h>
#include <tchar.h>

HINSTANCE hInst;
ITfThreadMgr *pThreadMgr = nullptr;
TfClientId clientId;
ITfDocumentMgr *docMgr_e1, *docMgr_e2;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    hInst = hInstance;
    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = _T("TSFMinimalWnd");

    RegisterClass(&wc);

    HWND hWnd = CreateWindow(wc.lpszClassName, _T("TSF Minimal App"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // TSF Initialize
    if (SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        if (SUCCEEDED(CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_ITfThreadMgr, (void**)&pThreadMgr))) {
            if (SUCCEEDED(pThreadMgr->Activate(&clientId))) {
                // get DocumentMgr
                pThreadMgr->CreateDocumentMgr(&docMgr_e1);
                pThreadMgr->CreateDocumentMgr(&docMgr_e2);
                MSG msg;
                while (GetMessage(&msg, nullptr, 0, 0)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                pThreadMgr->Deactivate();
                pThreadMgr->Release();
            }
        }
        CoUninitialize();
    }
    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static HWND hEdit1, hEdit2;
    switch (message) {
    case WM_CREATE:
        hEdit1 = CreateWindowEx(0, _T("EDIT"), _T(""),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
            10, 10, 360, 25, hWnd, nullptr, hInst, nullptr);
        hEdit2 = CreateWindowEx(0, _T("EDIT"), _T(""),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
            10, 40, 360, 25, hWnd, nullptr, hInst, nullptr);
        break;
    case WM_SETFOCUS:
        if (pThreadMgr == nullptr) break;
        if (hWnd == hEdit1) {
            pThreadMgr->SetFocus(docMgr_e1);
        } else if (hWnd == hEdit2) {
            pThreadMgr->SetFocus(docMgr_e2);
        } else {
            pThreadMgr->SetFocus(nullptr);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

