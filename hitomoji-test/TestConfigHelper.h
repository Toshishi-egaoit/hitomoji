#pragma once

#include <windows.h>
#include <fstream>
#include <string>
#include <vector>

class TempConfigDir {
public:
    explicit TempConfigDir(const wchar_t* prefix = L"hmj")
    {
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);

        wchar_t uniqueName[MAX_PATH];
        GetTempFileNameW(tempPath, prefix, 0, uniqueName);
        DeleteFileW(uniqueName);

        _baseDir = uniqueName;
        _baseDir += L"\\";
        CreateDirectoryW(_baseDir.c_str(), nullptr);
    }

    ~TempConfigDir()
    {
        for (const auto& file : _files) {
            DeleteFileW(file.c_str());
        }
        RemoveDirectoryW(_baseDir.c_str());
    }

    const std::wstring& BaseDir() const
    {
        return _baseDir;
    }

    void WriteFile(const std::wstring& fileName, const std::wstring& content)
    {
        std::wstring path = _baseDir + fileName;
        int size = WideCharToMultiByte(CP_UTF8, 0,
            content.c_str(), static_cast<int>(content.size()),
            nullptr, 0, nullptr, nullptr);
        std::string utf8(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0,
            content.c_str(), static_cast<int>(content.size()),
            utf8.data(), size, nullptr, nullptr);

        std::ofstream file(path, std::ios::binary);
        file.write(utf8.data(), utf8.size());
        file.close();
        _files.push_back(path);
    }

    void CopyFileFrom(const std::wstring& sourcePath, const std::wstring& fileName)
    {
        std::wstring path = _baseDir + fileName;
        CopyFileW(sourcePath.c_str(), path.c_str(), FALSE);
        _files.push_back(path);
    }

private:
    std::wstring _baseDir;
    std::vector<std::wstring> _files;
};
