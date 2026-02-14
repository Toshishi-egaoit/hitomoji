#pragma once
#include "utils.h"

// debug support functions
void OutputDebugStringWithInt(wchar_t const* format, ULONG lvalue)
{
    wchar_t buff[255];
    // TODO: assert‚Å255‚ð‰z‚¦‚½‚ç—Ž‚¿‚é‚æ‚¤‚É‚·‚é

    wsprintf(buff, format, lvalue);
    OutputDebugStringW(buff);
    return;
}

void OutputDebugStringWithString(wchar_t const* format, wchar_t const* value)
{
    wchar_t buff[255];
    // TODO: assert‚Å255‚ð‰z‚¦‚½‚ç—Ž‚¿‚é‚æ‚¤‚É‚·‚é

    wsprintf(buff, format, value);
    OutputDebugStringW(buff);
    return;
}

void OutputDebugStringWithGuid(wchar_t const* format, const IID iid)
{
    // TODO: assert‚Å255‚ð‰z‚¦‚½‚ç—Ž‚¿‚é‚æ‚¤‚É‚·‚é
    wchar_t buff[255];
    wchar_t guidStr[40] = {0}; // GUID•¶Žš—ñ—pƒoƒbƒtƒ@

    // GUID‚ð•¶Žš—ñ‚É•ÏŠ·
    StringFromGUID2(iid, guidStr, ARRAYSIZE(guidStr));

    wsprintf(buff, format, guidStr);
    OutputDebugStringW(buff);
    return;
}

void ChmLogger::_Output(const std::wstring& level, const std::wstring& msg)
{
#ifdef _DEBUG
    std::wstring line = L"[Hitomoji][" + level + L"] " + msg + L"\n";
    OutputDebugStringW(line.c_str());
#endif
}

void ChmLogger::Info(const std::wstring& msg)
{
    _Output(L"INFO", msg);
}

void ChmLogger::Warn(const std::wstring& msg)
{
    _Output(L"WARN", msg);
}

void ChmLogger::Error(const std::wstring& msg)
{
    _Output(L"ERROR", msg);
}

void ChmLogger::Debug(const std::wstring& msg)
{
    _Output(L"DEBUG", msg);
}

