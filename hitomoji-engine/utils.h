#pragma once
#include <windows.h>
#include <string>

// --- debug support functions
extern void OutputDebugStringWithInt(wchar_t const* format, ULONG lvalue);
extern void OutputDebugStringWithString(wchar_t const* format, wchar_t const* value);
extern void OutputDebugStringWithGuid(wchar_t const* format, const IID value);

class ChmLogger
{
public:
    static void Info(const std::wstring& msg);
    static void Error(const std::wstring& msg);
    static void Warn(const std::wstring& msg);
    static void Debug(const std::wstring& msg);
private:
	static void _Output(const std::wstring& level, const std::wstring& msg);
};

