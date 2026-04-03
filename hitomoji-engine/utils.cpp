#pragma once
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <assert.h>

#include "utils.h"

// debug support functions
void OutputDebugStringWithInt(wchar_t const* format, ULONG lvalue)
{
    wchar_t buff[255];
    // TODO: assertで255を越えたら落ちるようにする

    wsprintf(buff, format, lvalue);
    OutputDebugStringW(buff);
    return;
}

void OutputDebugStringWithString(wchar_t const* format, wchar_t const* value)
{
    wchar_t buff[255];
    // TODO: assertで255を越えたら落ちるようにする

    wsprintf(buff, format, value);
    OutputDebugStringW(buff);
    return;
}

void OutputDebugStringWithGuid(wchar_t const* format, const IID iid)
{
    // TODO: assertで255を越えたら落ちるようにする
    wchar_t buff[255];
    wchar_t guidStr[40] = {0}; // GUID文字列用バッファ

    // GUIDを文字列に変換
    StringFromGUID2(iid, guidStr, ARRAYSIZE(guidStr));

    wsprintf(buff, format, guidStr);
    OutputDebugStringW(buff);
    return;
}

// --- ChmLogger Implementation ---
ChmLogger::LogLevel ChmLogger::_logLevel = ChmLogger::LogLevel::InfoLevel; 

void ChmLogger::_Output(const std::wstring& level, const std::wstring& msg)
{
    std::wstring line = L"[" + level + L"] " + msg + L"\n";
#ifdef _DEBUG
    std::wstring line2 = L"[Hitomoji] " + line;
    OutputDebugStringW(line2.c_str());
#endif
	_RotateIfNeeded();
    _WriteFile(line);
}

void ChmLogger::_RotateIfNeeded()
{
	std::wstring logFile = _GetLogFile();
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

std::wstring& ChmLogger::_GetLogFile() {
	static std::wstring _logFile = GetBasePath()+ L"Hitomoji.log";
	return _logFile;
}

void ChmLogger::_WriteFile(const std::wstring& msg)
{
    std::wstring logFile = _GetLogFile();
    assert(!logFile.empty());

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

