#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>

#include "ChmCandidateWindow.h"

namespace {
constexpr wchar_t CHM_CANDIDATE_WINDOW_CLASS[] = L"HitomojiCandidateWindow";
constexpr UINT_PTR CHM_CANDIDATE_TIMER_ID = 1;
constexpr int CHM_CANDIDATE_COLS = 10;
constexpr int CHM_CANDIDATE_ROWS = 4;
constexpr int CHM_CANDIDATE_CELL_WIDTH = 34;
constexpr int CHM_CANDIDATE_CELL_HEIGHT = 30;
constexpr int CHM_CANDIDATE_PADDING = 10;
constexpr int CHM_CANDIDATE_ROW_OFFSETS[CHM_CANDIDATE_ROWS] = { 0, 17, 25, 34 };
constexpr int CHM_CANDIDATE_WIDTH = CHM_CANDIDATE_COLS * CHM_CANDIDATE_CELL_WIDTH + CHM_CANDIDATE_ROW_OFFSETS[3] + CHM_CANDIDATE_PADDING * 2;
constexpr int CHM_CANDIDATE_HEIGHT = CHM_CANDIDATE_ROWS * CHM_CANDIDATE_CELL_HEIGHT + CHM_CANDIDATE_PADDING * 2;
constexpr int CHM_CANDIDATE_HOME_F_INDEX = 23;
constexpr int CHM_CANDIDATE_HOME_J_INDEX = 26;
constexpr int CHM_CANDIDATE_MARKER_HEIGHT = 2;
constexpr int CHM_CANDIDATE_HOME_MARKER_HEIGHT = 4;

int Utf32ToUtf16(uint32_t cp, wchar_t out[3])
{
    out[0] = L'\0';
    out[1] = L'\0';
    out[2] = L'\0';

    if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        return 0;
    }

    if (cp <= 0xFFFF) {
        out[0] = static_cast<wchar_t>(cp);
        return 1;
    }

    cp -= 0x10000;
    out[0] = static_cast<wchar_t>(0xD800 + (cp >> 10));
    out[1] = static_cast<wchar_t>(0xDC00 + (cp & 0x3FF));
    return 2;
}

void DrawCandidates(HDC hdc, const ChmCandidatePage* page)
{
    if (!page) return;

    int fontHeight = -MulDiv(16, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    HFONT font = CreateFontW(
        fontHeight,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Yu Gothic UI");
    HFONT oldFont = font ? static_cast<HFONT>(SelectObject(hdc, font)) : nullptr;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

    HBRUSH markerBrush = CreateSolidBrush(RGB(192, 192, 192));
    HBRUSH homeMarkerBrush = CreateSolidBrush(RGB(96, 96, 96));

    for (int i = 0; i < static_cast<int>(CHM_CANDIDATE_MAX); ++i) {
        int row = i / CHM_CANDIDATE_COLS;
        int col = i % CHM_CANDIDATE_COLS;
        RECT cell{
            CHM_CANDIDATE_PADDING + CHM_CANDIDATE_ROW_OFFSETS[row] + col * CHM_CANDIDATE_CELL_WIDTH,
            CHM_CANDIDATE_PADDING + row * CHM_CANDIDATE_CELL_HEIGHT,
            CHM_CANDIDATE_PADDING + CHM_CANDIDATE_ROW_OFFSETS[row] + (col + 1) * CHM_CANDIDATE_CELL_WIDTH,
            CHM_CANDIDATE_PADDING + (row + 1) * CHM_CANDIDATE_CELL_HEIGHT
        };

        bool isHome = (i == CHM_CANDIDATE_HOME_F_INDEX || i == CHM_CANDIDATE_HOME_J_INDEX);
        int markerHeight = isHome ? CHM_CANDIDATE_HOME_MARKER_HEIGHT : CHM_CANDIDATE_MARKER_HEIGHT;
        RECT marker{
            cell.left + 5,
            cell.bottom - 5,
            cell.right - 5,
            cell.bottom - 5 + markerHeight
        };
        FillRect(hdc, &marker, isHome ? homeMarkerBrush : markerBrush);

        uint32_t cp = page->candidates[i];
        if (cp == 0) continue;

        wchar_t text[3];
        if (Utf32ToUtf16(cp, text) == 0) continue;

        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        DrawTextW(hdc, text, -1, &cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    if (homeMarkerBrush) {
        DeleteObject(homeMarkerBrush);
    }
    if (markerBrush) {
        DeleteObject(markerBrush);
    }

    if (oldFont) {
        SelectObject(hdc, oldFont);
    }
    if (font) {
        DeleteObject(font);
    }
}
}

ChmCandidateWindowThread::ChmCandidateWindowThread()
    : _hInst(nullptr),
      _hThread(nullptr),
      _hReadyEvent(nullptr),
      _threadId(0),
      _hwnd(nullptr),
      _pendingPage(nullptr),
      _state(State::Hidden)
{
}

ChmCandidateWindowThread::~ChmCandidateWindowThread()
{
    Stop();
}

BOOL ChmCandidateWindowThread::Start(HINSTANCE hInst)
{
    if (_hThread) return TRUE;

    _hInst = hInst;
    _hReadyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!_hReadyEvent) return FALSE;

    _hThread = CreateThread(nullptr, 0, ThreadProc, this, 0, &_threadId);
    if (!_hThread) {
        CloseHandle(_hReadyEvent);
        _hReadyEvent = nullptr;
        _threadId = 0;
        return FALSE;
    }

    DWORD waitResult = WaitForSingleObject(_hReadyEvent, 5000);
    CloseHandle(_hReadyEvent);
    _hReadyEvent = nullptr;

    if (waitResult != WAIT_OBJECT_0 || !_hwnd) {
        Stop();
        return FALSE;
    }

    return TRUE;
}

void ChmCandidateWindowThread::Stop()
{
    if (!_hThread) return;

    if (_threadId != 0) {
        PostThreadMessageW(_threadId, WM_HM_CAND_QUIT, 0, 0);
    }

    WaitForSingleObject(_hThread, 5000);
    CloseHandle(_hThread);
    _hThread = nullptr;
    _threadId = 0;
    _hwnd = nullptr;
    _hInst = nullptr;
}

BOOL ChmCandidateWindowThread::ScheduleShow(const ChmCandidatePage& page)
{
    if (!_hThread || _threadId == 0) return FALSE;

    ChmCandidatePage* payload = new ChmCandidatePage(page);
    if (!PostThreadMessageW(_threadId, WM_HM_CAND_SCHEDULE_SHOW, 0, reinterpret_cast<LPARAM>(payload))) {
        delete payload;
        return FALSE;
    }
    return TRUE;
}

BOOL ChmCandidateWindowThread::Hide()
{
    if (!_hThread || _threadId == 0) return FALSE;
    return PostThreadMessageW(_threadId, WM_HM_CAND_HIDE, 0, 0);
}

DWORD WINAPI ChmCandidateWindowThread::ThreadProc(LPVOID param)
{
    return static_cast<ChmCandidateWindowThread*>(param)->_Run();
}

DWORD ChmCandidateWindowThread::_Run()
{
    _threadId = GetCurrentThreadId();

    BOOL created = _CreateWindow();
    if (_hReadyEvent) {
        SetEvent(_hReadyEvent);
    }
    if (!created) return 1;

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        switch (msg.message) {
        case WM_HM_CAND_SCHEDULE_SHOW:
            _OnScheduleShow(reinterpret_cast<ChmCandidatePage*>(msg.lParam));
            break;
        case WM_HM_CAND_HIDE:
            _OnHide();
            break;
        case WM_HM_CAND_QUIT:
            _OnHide();
            DestroyWindow(_hwnd);
            _hwnd = nullptr;
            UnregisterClassW(CHM_CANDIDATE_WINDOW_CLASS, _hInst);
            return 0;
        default:
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            break;
        }
    }

    _DiscardPendingPage();
    if (_hwnd) {
        DestroyWindow(_hwnd);
        _hwnd = nullptr;
    }
    UnregisterClassW(CHM_CANDIDATE_WINDOW_CLASS, _hInst);
    return 0;
}

BOOL ChmCandidateWindowThread::_CreateWindow()
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = _hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = CHM_CANDIDATE_WINDOW_CLASS;

    RegisterClassExW(&wc);

    _hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        CHM_CANDIDATE_WINDOW_CLASS,
        L"Hitomoji Candidate",
        WS_POPUP | WS_BORDER,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CHM_CANDIDATE_WIDTH,
        CHM_CANDIDATE_HEIGHT,
        nullptr,
        nullptr,
        _hInst,
        this);

    if (!_hwnd) return FALSE;

    ShowWindow(_hwnd, SW_HIDE);
    return TRUE;
}

void ChmCandidateWindowThread::_OnScheduleShow(ChmCandidatePage* page)
{
    if (!page || !_hwnd) {
        delete page;
        return;
    }

    KillTimer(_hwnd, CHM_CANDIDATE_TIMER_ID);
    _DiscardPendingPage();
    _pendingPage = page;

    if (_pendingPage->delayMs == 0) {
        _ShowPendingPage();
        return;
    }

    SetTimer(_hwnd, CHM_CANDIDATE_TIMER_ID, _pendingPage->delayMs, nullptr);
    _state = State::WaitingToShow;
}

void ChmCandidateWindowThread::_OnHide()
{
    if (_hwnd) {
        KillTimer(_hwnd, CHM_CANDIDATE_TIMER_ID);
        ShowWindow(_hwnd, SW_HIDE);
    }
    _DiscardPendingPage();
    _state = State::Hidden;
}

void ChmCandidateWindowThread::_ShowPendingPage()
{
    if (!_hwnd || !_pendingPage) return;

    KillTimer(_hwnd, CHM_CANDIDATE_TIMER_ID);

    RECT rc = _pendingPage->anchorRect;
    int x = rc.left;
    int y = rc.bottom;

    HMONITOR monitor = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(monitor, &mi)) {
        int left = static_cast<int>(mi.rcWork.left);
        int top = static_cast<int>(mi.rcWork.top);
        int right = static_cast<int>(mi.rcWork.right) - CHM_CANDIDATE_WIDTH;
        int bottom = static_cast<int>(mi.rcWork.bottom) - CHM_CANDIDATE_HEIGHT;
        if (x < left) x = left;
        if (x > right) x = right;
        if (y < top) y = top;
        if (y > bottom) y = bottom;
    }

    SetWindowPos(
        _hwnd,
        HWND_TOPMOST,
        x,
        y,
        CHM_CANDIDATE_WIDTH,
        CHM_CANDIDATE_HEIGHT,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(_hwnd, nullptr, TRUE);
    _state = State::Visible;
}

void ChmCandidateWindowThread::_DiscardPendingPage()
{
    delete _pendingPage;
    _pendingPage = nullptr;
}

LRESULT CALLBACK ChmCandidateWindowThread::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    ChmCandidateWindowThread* self = reinterpret_cast<ChmCandidateWindowThread*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<ChmCandidateWindowThread*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    switch (msg) {
    case WM_TIMER:
        if (self && wp == CHM_CANDIDATE_TIMER_ID) {
            self->_ShowPendingPage();
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        FrameRect(hdc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOWFRAME + 1));
        if (self) {
            DrawCandidates(hdc, self->_pendingPage);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        if (self) {
            self->_DiscardPendingPage();
        }
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}
