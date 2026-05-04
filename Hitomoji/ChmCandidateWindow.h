#pragma once

#include <windows.h>
#include "ChmCandidatePage.h"

constexpr UINT WM_HM_CAND_SCHEDULE_SHOW = WM_APP + 0x420;
constexpr UINT WM_HM_CAND_HIDE = WM_APP + 0x421;
constexpr UINT WM_HM_CAND_QUIT = WM_APP + 0x422;

class ChmCandidateWindowThread {
public:
    ChmCandidateWindowThread();
    ~ChmCandidateWindowThread();

    BOOL Start(HINSTANCE hInst);
    void Stop();

    BOOL ScheduleShow(const ChmCandidatePage& page);
    BOOL Hide();

private:
    enum class State {
        Hidden,
        WaitingToShow,
        Visible,
    };

    static DWORD WINAPI ThreadProc(LPVOID param);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    DWORD _Run();
    BOOL _CreateWindow();
    void _OnScheduleShow(ChmCandidatePage* page);
    void _OnHide();
    void _ShowPendingPage();
    void _DiscardPendingPage();

    HINSTANCE _hInst;
    HANDLE _hThread;
    HANDLE _hReadyEvent;
    DWORD _threadId;
    HWND _hwnd;
    ChmCandidatePage* _pendingPage;
    State _state;
};
