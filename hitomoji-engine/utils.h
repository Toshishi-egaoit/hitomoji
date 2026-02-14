#pragma once
#include <windows.h>

// --- debug support functions
extern void OutputDebugStringWithInt(wchar_t const* format, ULONG lvalue);
extern void OutputDebugStringWithString(wchar_t const* format, wchar_t const* value);
extern void OutputDebugStringWithGuid(wchar_t const* format, const IID value);
