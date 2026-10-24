#pragma once
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <assert.h>

#include "utils.h"
#include "ChmEnvironment.h"

// --- ChmLogger Implementation ---
ChmLogger::LogLevel ChmLogger::_logLevel = ChmLogger::LogLevel::InfoLevel; 

bool ChmLogger::_InspectDbgViewProcess()
{
	WCHAR path[MAX_PATH] = {};

	DWORD len = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
	if (len == 0 || len >= ARRAYSIZE(path)) return false;

	LPCWSTR fileName = path;
	for (LPCWSTR p = path; *p; ++p) {
		if (*p == L'\\' || *p == L'/') {
			fileName = p + 1;
		}
	}
	return  lstrcmpiW(fileName, L"Dbgview.exe") == 0 ||
		lstrcmpiW(fileName, L"Dbgview64.exe") == 0 ||
		lstrcmpiW(fileName, L"Dbgview64a.exe") == 0;
}

bool ChmLogger::IsDbgViewProcess()
{
    static const bool isDbgView = _InspectDbgViewProcess();
    return isDbgView;
}


void ChmLogger::_Output(const std::wstring& level, const std::wstring& msg)
{
	std::wstring line = L"[" + level + L"] " + msg + L"\n";
	_RotateIfNeeded();
	_WriteFile(line);

#ifdef _DEBUG
	if (!IsDbgViewProcess())
	{
		std::wstring line2 = L"[Hitomoji] " + line;
		//OutputDebugStringW(line2.c_str());
	}
#endif
}

void ChmLogger::_RotateIfNeeded()
{
	std::wstring logFile = _GetLogFile();
	if (logFile.empty()) return;
	WIN32_FILE_ATTRIBUTE_DATA fad;
	if (GetFileAttributesExW(logFile.c_str(), GetFileExInfoStandard, &fad))
	{
		ULARGE_INTEGER size;
		size.HighPart = fad.nFileSizeHigh;
		size.LowPart  = fad.nFileSizeLow;

		if (size.QuadPart > MAX_LOG_SIZE)
		{
			std::wstring oldFile = logFile + L".old";
			DeleteFileW(oldFile.c_str());
			MoveFileW(logFile.c_str(), oldFile.c_str());
		}
	}
}

std::wstring ChmLogger::_GetLogFile() {
	std::wstring basePath = g_environment.GetBasePath();
	if (basePath.empty()) return {};
	if (basePath.back() != L'\\' && basePath.back() != L'/') basePath += L'\\';
	return basePath + L"Hitomoji.log";
}

void ChmLogger::_WriteFile(const std::wstring& msg)
{
	std::wstring logFile = _GetLogFile();
	// assert(!logFile.empty());
	if (logFile.empty()) return;

	HANDLE hFile = CreateFileW(
		logFile.c_str(),
		FILE_APPEND_DATA,
		FILE_SHARE_READ,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		// ===== 新規ファイル判定(BOM書き込みのため =====
		DWORD fileSize = GetFileSize(hFile, NULL);

		if (fileSize == 0)
		{
			// UTF-16LE BOM
			const wchar_t bom = 0xFEFF;
			DWORD written;
			WriteFile(hFile, &bom, sizeof(bom), &written, NULL);
		}

		// ===== 日時生成 =====
		SYSTEMTIME st;
		GetLocalTime(&st);

		wchar_t timeBuf[64];
		swprintf_s(
			timeBuf,
			L"%04d-%02d-%02d %02d:%02d:%02d.%03d ",
			st.wYear,
			st.wMonth,
			st.wDay,
			st.wHour,
			st.wMinute,
			st.wSecond,
			st.wMilliseconds);

		std::wstring line = timeBuf;
		line += msg;
		if (line.back() != L'\n')
			line += L"\r\n";

		DWORD written;
		WriteFile(
			hFile,
			line.c_str(),
			(DWORD)(line.size() * sizeof(wchar_t)),
			&written,
			NULL);

		CloseHandle(hFile);
	}
}

void ChmLogger::Info(const std::wstring& msg)
{
	if (_logLevel < LogLevel::InfoLevel) return;
	_Output(L"INFO", msg);
}

void ChmLogger::Warn(const std::wstring& msg)
{
	if (_logLevel < LogLevel::WarnLevel) return;
	_Output(L"WARN", msg);
}

void ChmLogger::Error(const std::wstring& msg)
{
	// Errorは常に出力する
	_Output(L"ERROR", msg);
}

void ChmLogger::Debug(const std::wstring& msg)
{
	if (_logLevel < LogLevel::DebugLevel) return;
	_Output(L"DEBUG", msg);
}

std::wstring ChmLogger::Format(const wchar_t* fmt, ...)
{
    wchar_t buf[512];

    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, args);
    va_end(args);

    return std::wstring(buf);
}
std::string ToNarrow(const std::wstring& ws)
{
	std::string s;
	s.reserve(ws.size());
	for (wchar_t wc : ws) {
		if (wc <= 0x7F) {
			s.push_back(static_cast<char>(wc));
		} else {
			Warn(Format(L"ToNarrow: non-ASCII char U+%04X ignored", wc));
		}
	}
	return s;
}
