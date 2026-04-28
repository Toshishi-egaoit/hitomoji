#pragma once
#include <windows.h>
#include <string>

// --- debug support functions
extern void OutputDebugStringWithInt(wchar_t const* format, ULONG lvalue);
extern void OutputDebugStringWithString(wchar_t const* format, wchar_t const* value);
extern void OutputDebugStringWithGuid(wchar_t const* format, const IID value);
extern std::string ToNarrow(const std::wstring& ws);

class ChmLogger
{
public:
	static const DWORD MAX_LOG_SIZE = 1024 * 1024; // 1MB
	enum LogLevel {
		ErrorLevel,
		WarnLevel,
		InfoLevel,
		DebugLevel,
	} ;
	static void SetLogLevel(LogLevel level) { _logLevel = level; }
    static void Error(const std::wstring& msg);
    static void Warn(const std::wstring& msg);
    static void Info(const std::wstring& msg);
    static void Debug(const std::wstring& msg);
    static std::wstring  Format(const wchar_t* fmt, ...);
private:
	static void _Output(const std::wstring& level, const std::wstring& msg);
	static std::wstring& _GetLogFile();
	static void _RotateIfNeeded();
	static void _WriteFile(const std::wstring& line);

	static std::wstring _logBase ;
	static enum LogLevel _logLevel ;
};

#define Debug(msg) ChmLogger::Debug(msg)
#define Info(msg) ChmLogger::Info(msg)
#define Warn(msg) ChmLogger::Warn(msg)
#define Error(msg) ChmLogger::Error(msg)
#define Format(form, ...) ChmLogger::Format(form, __VA_ARGS__)

