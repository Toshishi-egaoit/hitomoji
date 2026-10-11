#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <string>

#include "ChmCandidateWindow.h"
#include "ChmCandidateTooltip.h"

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

HFONT TooltipFont(HDC hdc, int points)
{
    return CreateFontW(-MulDiv(points, GetDeviceCaps(hdc, LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Yu Gothic UI");
}

// Keyboard order: left little/ring/middle/index, right index/middle/ring/little.
// Keep separate entries for all eight fingers for future configuration support.
constexpr COLORREF CHM_CANDIDATE_FINGER_COLORS[8] = {
    RGB(64, 64, 64), // Left little
    RGB(34, 100, 64),   // Left ring: dark green
    RGB(64, 64, 64), // Left middle
    RGB(32, 64, 112),   // Left index: navy
    RGB(128, 72, 40),   // Right index: brown
    RGB(64, 64, 64), // Right middle
    RGB(34, 100, 64),   // Right ring: dark green
    RGB(64, 64, 64), // Right little
};
// Each index finger covers two columns (R/T and Y/U, F/G and H/J, V/B and N/M).
constexpr int CHM_CANDIDATE_COLUMN_FINGERS[CHM_CANDIDATE_COLS] = {
    0, 1, 2, 3, 3, 4, 4, 5, 6, 7
};

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
    HFONT variantFont = TooltipFont(hdc, 10);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

    HBRUSH markerBrushes[8]{};
    for (int finger = 0; finger < 8; ++finger) {
        markerBrushes[finger] = CreateSolidBrush(CHM_CANDIDATE_FINGER_COLORS[finger]);
    }

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
        FillRect(hdc, &marker, markerBrushes[CHM_CANDIDATE_COLUMN_FINGERS[col]]);

        uint32_t cp = page->candidates[i];
        if (cp == 0) continue;

        wchar_t text[3];
        if (Utf32ToUtf16(cp, text) == 0) continue;

        SetTextColor(hdc, CHM_CANDIDATE_FINGER_COLORS[CHM_CANDIDATE_COLUMN_FINGERS[col]]);
        DrawTextW(hdc, text, -1, &cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        const bool hasVariants = std::any_of(page->kanjiInfo.begin(), page->kanjiInfo.end(),
            [i, cp](const ChmKanjiInfo& info) {
                return info.cellIndex == static_cast<size_t>(i) && info.codePoint == cp
                    && !info.variants.empty();
            });
        if (hasVariants && variantFont) {
            HGDIOBJ previousFont = SelectObject(hdc, variantFont);
            // Move toward the kanji, but lower the enlarged indicator to keep it clear.
            RECT indicator{cell.left, cell.top, cell.right, cell.bottom + 3};
            DrawTextW(hdc, L"v", -1, &indicator, DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(hdc, previousFont);
        }
    }

    if (variantFont) DeleteObject(variantFont);

    for (HBRUSH brush : markerBrushes) {
        if (brush) DeleteObject(brush);
    }

    if (page->totalPages > 0) {
        HFONT pageFont = CreateFontW(
            -MulDiv(12, GetDeviceCaps(hdc, LOGPIXELSY), 72),
            0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Yu Gothic UI");
        HFONT previousFont = pageFont ? static_cast<HFONT>(SelectObject(hdc, pageFont)) : nullptr;
        const std::wstring label = std::to_wstring(page->page + 1) + L"/" + std::to_wstring(page->totalPages);
        RECT pageRect{
            CHM_CANDIDATE_PADDING + CHM_CANDIDATE_COLS * CHM_CANDIDATE_CELL_WIDTH,
            CHM_CANDIDATE_PADDING,
            CHM_CANDIDATE_WIDTH - CHM_CANDIDATE_PADDING,
            CHM_CANDIDATE_PADDING + CHM_CANDIDATE_CELL_HEIGHT
        };
        SetTextColor(hdc, RGB(64, 64, 64));
        DrawTextW(hdc, label.c_str(), -1, &pageRect,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (previousFont) SelectObject(hdc, previousFont);
        if (pageFont) DeleteObject(pageFont);
    }

    if (oldFont) {
        SelectObject(hdc, oldFont);
    }
    if (font) {
        DeleteObject(font);
    }
}
}

ChmCandidateWindowThread::ChmCandidateWindowThread(ChmCandidatePositionProvider positionProvider, void* positionContext)
    : _hInst(nullptr),
      _hThread(nullptr),
      _hReadyEvent(nullptr),
      _threadId(0),
      _hwnd(nullptr),
      _pendingPage(nullptr),
      _pendingDelayMs(0),
      _positionProvider(positionProvider),
      _positionContext(positionContext),
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

BOOL ChmCandidateWindowThread::ScheduleShow(const ChmCandidatePage& page, DWORD delayMs)
{
    if (!_hThread || _threadId == 0) return FALSE;

    ChmCandidatePage* payload = new ChmCandidatePage(page);
    if (!PostThreadMessageW(_threadId, WM_HM_CAND_SCHEDULE_SHOW, delayMs, reinterpret_cast<LPARAM>(payload))) {
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
            _OnScheduleShow(reinterpret_cast<ChmCandidatePage*>(msg.lParam), static_cast<DWORD>(msg.wParam));
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

    _tooltip = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        CHM_CANDIDATE_WINDOW_CLASS, L"Hitomoji Kanji Info", WS_POPUP,
        0, 0, 0, 0, _hwnd, nullptr, _hInst, this);

    ShowWindow(_hwnd, SW_HIDE);
    return TRUE;
}

void ChmCandidateWindowThread::_OnScheduleShow(ChmCandidatePage* page, DWORD delayMs)
{
    if (!page || !_hwnd) {
        delete page;
        return;
    }

    KillTimer(_hwnd, CHM_CANDIDATE_TIMER_ID);
    _DiscardPendingPage();
    _pendingPage = page;
    _pendingDelayMs = delayMs;

    if (_pendingDelayMs == 0) {
        _ShowPendingPage();
        return;
    }

    SetTimer(_hwnd, CHM_CANDIDATE_TIMER_ID, _pendingDelayMs, nullptr);
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

    POINT pt{};
    if (!_positionProvider || !_positionProvider(_positionContext, pt)) {
        GetCursorPos(&pt);
    }

    RECT rc{ pt.x, pt.y, pt.x + 1, pt.y + 1 };
    int x = pt.x;
    int y = pt.y;

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
    _HideTooltip();
    delete _pendingPage;
    _pendingPage = nullptr;
}

void ChmCandidateWindowThread::_HideTooltip()
{
    if (_tooltip) ShowWindow(_tooltip, SW_HIDE);
    if (_hwnd) {
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_CANCEL | TME_HOVER | TME_LEAVE, _hwnd, 0};
        TrackMouseEvent(&tracking);
    }
    _hoverCell = -1;
    _tooltipTitle.clear();
    _tooltipVariants.clear();
    _tooltipBody.clear();
}

int ChmCandidateWindowThread::_HitTest(POINT point) const
{
    if (_state != State::Visible || !_pendingPage) return -1;
    const int y = point.y - CHM_CANDIDATE_PADDING;
    if (y < 0 || y >= CHM_CANDIDATE_ROWS * CHM_CANDIDATE_CELL_HEIGHT) return -1;
    const int row = y / CHM_CANDIDATE_CELL_HEIGHT;
    const int x = point.x - CHM_CANDIDATE_PADDING - CHM_CANDIDATE_ROW_OFFSETS[row];
    if (x < 0 || x >= CHM_CANDIDATE_COLS * CHM_CANDIDATE_CELL_WIDTH) return -1;
    const int cell = row * CHM_CANDIDATE_COLS + x / CHM_CANDIDATE_CELL_WIDTH;
    return _pendingPage->candidates[cell] ? cell : -1;
}

void ChmCandidateWindowThread::_OnMouseMove(POINT point)
{
    const int cell = _HitTest(point);
    if (cell == _hoverCell) return;
    _HideTooltip();
    _hoverCell = cell;
    if (cell >= 0) {
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_HOVER | TME_LEAVE, _hwnd, 500};
        TrackMouseEvent(&tracking);
    }
}

void ChmCandidateWindowThread::_ShowTooltip()
{
    if (!_tooltip || !_pendingPage || _hoverCell < 0 || _state != State::Visible) return;
    POINT cursor{};
    if (!GetCursorPos(&cursor)) return;
    POINT local = cursor;
    ScreenToClient(_hwnd, &local);
    if (_HitTest(local) != _hoverCell) { _HideTooltip(); return; }
    ChmKanjiInfo info;
    info.codePoint = _pendingPage->candidates[_hoverCell];
    for (const auto& value : _pendingPage->kanjiInfo) {
        if (value.cellIndex == static_cast<size_t>(_hoverCell) && value.codePoint == info.codePoint) {
            info = value;
            break;
        }
    }
    _tooltipTitle = ChmCandidateTooltip::Title(info);
    _tooltipVariants = ChmCandidateTooltip::Variants(info);
    _tooltipBody = ChmCandidateTooltip::Body(info);
    if (_tooltipTitle.empty()) return;

    MONITORINFO monitor{sizeof(monitor)};
    if (!GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &monitor)) return;
    HDC hdc = GetDC(_tooltip);
    if (!hdc) return;
    HFONT titleFont = TooltipFont(hdc, 24);
    HFONT bodyFont = TooltipFont(hdc, 12);
    HGDIOBJ oldFont = GetCurrentObject(hdc, OBJ_FONT);
    const int maxWidth = (std::max)(1, (std::min)(420, static_cast<int>(monitor.rcWork.right - monitor.rcWork.left) - 24));
    RECT title{0, 0, maxWidth, 0}, variants{0, 0, (std::max)(1, maxWidth - 16), 0}, body{0, 0, maxWidth, 0};
    RECT variantLabel{};
    if (titleFont) SelectObject(hdc, titleFont);
    DrawTextW(hdc, _tooltipTitle.c_str(), -1, &title, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    if (!_tooltipVariants.empty()) DrawTextW(hdc, _tooltipVariants.c_str(), -1, &variants, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    if (bodyFont) SelectObject(hdc, bodyFont);
    if (!_tooltipVariants.empty()) DrawTextW(hdc, L"異体字：", -1, &variantLabel, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    if (!_tooltipBody.empty()) DrawTextW(hdc, _tooltipBody.c_str(), -1, &body, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    _tooltipTitleHeight = title.bottom;
    _tooltipVariantsHeight = variants.bottom;
    _tooltipVariantLabelHeight = variantLabel.bottom;
    const LONG variantWidth = _tooltipVariants.empty() ? 0 : (std::max)(variantLabel.right, variants.right + 16);
    const int width = (std::max)(title.right, (std::max)(variantWidth, body.right)) + 24;
    const int height = title.bottom + 24
        + (_tooltipVariants.empty() ? 0 : variantLabel.bottom + variants.bottom + 12)
        + (_tooltipBody.empty() ? 0 : body.bottom + 6);
    SelectObject(hdc, oldFont);
    if (titleFont) DeleteObject(titleFont);
    if (bodyFont) DeleteObject(bodyFont);
    ReleaseDC(_tooltip, hdc);

    RECT candidate{};
    GetWindowRect(_hwnd, &candidate);
    int x = candidate.right + 8, y = cursor.y;
    if (x + width > monitor.rcWork.right) x = candidate.left - width - 8;
    if (x < monitor.rcWork.left) {
        x = candidate.left;
        y = candidate.bottom + 8;
        if (y + height > monitor.rcWork.bottom) y = candidate.top - height - 8;
    }
    x = (std::max)(static_cast<int>(monitor.rcWork.left), (std::min)(x, static_cast<int>(monitor.rcWork.right) - width));
    y = (std::max)(static_cast<int>(monitor.rcWork.top), (std::min)(y, static_cast<int>(monitor.rcWork.bottom) - height));
    SetWindowPos(_tooltip, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(_tooltip, nullptr, TRUE);
}

void ChmCandidateWindowThread::_PaintTooltip(HDC hdc)
{
    RECT bounds{};
    GetClientRect(_tooltip, &bounds);
    FillRect(hdc, &bounds, GetSysColorBrush(COLOR_INFOBK));
    FrameRect(hdc, &bounds, GetSysColorBrush(COLOR_WINDOWFRAME));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor(COLOR_INFOTEXT));
    HFONT titleFont = TooltipFont(hdc, 24), bodyFont = TooltipFont(hdc, 12);
    HGDIOBJ oldFont = GetCurrentObject(hdc, OBJ_FONT);
    RECT title{12, 12, bounds.right - 12, 12 + _tooltipTitleHeight};
    RECT variantLabel{12, title.bottom + 12, bounds.right - 12, title.bottom + 12 + _tooltipVariantLabelHeight};
    RECT variants{28, variantLabel.bottom, bounds.right - 12, variantLabel.bottom + _tooltipVariantsHeight};
    RECT body{12, (_tooltipVariants.empty() ? title.bottom : variants.bottom) + 6, bounds.right - 12, bounds.bottom - 12};
    if (titleFont) SelectObject(hdc, titleFont);
    DrawTextW(hdc, _tooltipTitle.c_str(), -1, &title, DT_SINGLELINE | DT_NOPREFIX);
    if (!_tooltipVariants.empty()) DrawTextW(hdc, _tooltipVariants.c_str(), -1, &variants, DT_WORDBREAK | DT_NOPREFIX);
    if (bodyFont) SelectObject(hdc, bodyFont);
    if (!_tooltipVariants.empty()) DrawTextW(hdc, L"異体字：", -1, &variantLabel, DT_SINGLELINE | DT_NOPREFIX);
    DrawTextW(hdc, _tooltipBody.c_str(), -1, &body, DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
    if (titleFont) DeleteObject(titleFont);
    if (bodyFont) DeleteObject(bodyFont);
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
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATEANDEAT;
    case WM_NCHITTEST:
        if (self && hwnd == self->_tooltip) return HTTRANSPARENT;
        break;
    case WM_MOUSEMOVE:
        if (self && hwnd == self->_hwnd)
            self->_OnMouseMove({static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))});
        return 0;
    case WM_MOUSEHOVER:
        if (self && hwnd == self->_hwnd) self->_ShowTooltip();
        return 0;
    case WM_MOUSELEAVE:
        if (self && hwnd == self->_hwnd) self->_HideTooltip();
        return 0;
    case WM_TIMER:
        if (self && wp == CHM_CANDIDATE_TIMER_ID) {
            self->_ShowPendingPage();
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (self && hwnd == self->_tooltip) {
            self->_PaintTooltip(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
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
        if (self && hwnd == self->_tooltip) {
            self->_tooltip = nullptr;
            return 0;
        }
        if (self) {
            self->_DiscardPendingPage();
        }
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}
