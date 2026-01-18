#pragma once
#include <Windows.h>

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
