// Minimal TSF-enabled Win32 application with a single-line edit box
#include <windows.h>
#include <msctf.h>
#include <tchar.h>

HINSTANCE hInst;
ITfThreadMgr *pThreadMgr = nullptr;
TfClientId clientId;
ITfDocumentMgr *docMgr_e1, *docMgr_e2;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

namespace {
	constexpr INT_PTR FontNameId = 101;
	constexpr INT_PTR FontSizeId = 102;
	const wchar_t* const FontNames[] = { L"游明朝", L"游ゴシック", L"Yu Gothic UI", L"メイリオ", L"ＭＳ 明朝", L"ＭＳ ゴシック" };
	constexpr int FontSizes[] = { 12, 16, 20, 24, 28, 32, 36 };

	void ApplyEditFont(HWND window, HWND edit, HWND names, HWND sizes,
					   HWND checkbox, HWND button, HFONT& currentFont)
	{
		const LRESULT name = SendMessage(names, CB_GETCURSEL, 0, 0);
		const LRESULT size = SendMessage(sizes, CB_GETCURSEL, 0, 0);
		if (name == CB_ERR || size == CB_ERR) return;
		HDC dc = GetDC(window);
		if (!dc) return;
		HFONT next = CreateFontW(-MulDiv(FontSizes[size], GetDeviceCaps(dc, LOGPIXELSY), 72),
			0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE, FontNames[name]);
		if (!next) { ReleaseDC(window, dc); return; }
		HGDIOBJ previous = SelectObject(dc, next);
		TEXTMETRIC metrics{};
		GetTextMetrics(dc, &metrics);
		SelectObject(dc, previous);
		ReleaseDC(window, dc);

		const int editHeight = metrics.tmHeight + 12;
		MoveWindow(edit, 10, 90, 360, editHeight, TRUE);
		MoveWindow(checkbox, 10, 90 + editHeight + 15, 80, 25, TRUE);
		MoveWindow(button, 210, 90 + editHeight + 15, 100, 25, TRUE);
		RECT bounds{0, 0, 380, 90 + editHeight + 50};
		AdjustWindowRectEx(&bounds, static_cast<DWORD>(GetWindowLongPtr(window, GWL_STYLE)),
			FALSE, static_cast<DWORD>(GetWindowLongPtr(window, GWL_EXSTYLE)));
		SetWindowPos(window, nullptr, 0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top,
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		SendMessage(edit, WM_SETFONT, reinterpret_cast<WPARAM>(next), TRUE);
		if (currentFont) DeleteObject(currentFont);
		currentFont = next;
	}
}

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
					if (!IsDialogMessage(hWnd, &msg)){
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
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
    static HWND hEdit1, hEdit2, hBtn1, hBtn2, hFontName, hFontSize;
    static HFONT hEditFont = nullptr;
    switch (message) {
    case WM_CREATE: {
        hFontName = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            10, 10, 240, 220, hWnd, reinterpret_cast<HMENU>(FontNameId), hInst, nullptr);
        hFontSize = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            260, 10, 110, 220, hWnd, reinterpret_cast<HMENU>(FontSizeId), hInst, nullptr);
        for (const auto* name : FontNames)
            SendMessageW(hFontName, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
        for (int size : FontSizes) {
            wchar_t label[16];
            wsprintfW(label, L"%d pt", size);
            SendMessageW(hFontSize, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
        }
        SendMessage(hFontName, CB_SETCURSEL, 0, 0);
        SendMessage(hFontSize, CB_SETCURSEL, 3, 0); // 24 pt
        hEdit1 = CreateWindowEx(0, _T("EDIT"), _T(""),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
            10, 50, 360, 25, hWnd, nullptr, hInst, nullptr);
        hEdit2 = CreateWindowEx(0, _T("EDIT"), _T(""),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
            10, 90, 360, 50, hWnd, nullptr, hInst, nullptr);
        hBtn1 = CreateWindowEx(0, _T("BUTTON"), _T(""),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX,
            10, 130, 80, 25, hWnd, nullptr, hInst, nullptr);
        hBtn2 = CreateWindowEx(0, _T("BUTTON"), _T(""),
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | BS_PUSHBUTTON,
            210, 130, 100, 25, hWnd, nullptr, hInst, nullptr);
        ApplyEditFont(hWnd, hEdit2, hFontName, hFontSize, hBtn1, hBtn2, hEditFont);
		break;
    }
    case WM_COMMAND:
        if (HIWORD(wParam) == CBN_SELCHANGE &&
            (LOWORD(wParam) == FontNameId || LOWORD(wParam) == FontSizeId)) {
            ApplyEditFont(hWnd, hEdit2, hFontName, hFontSize, hBtn1, hBtn2, hEditFont);
        }
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
        if (hEditFont) {
            DeleteObject(hEditFont);
            hEditFont = nullptr;
        }
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

