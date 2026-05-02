#pragma once
#include "ChmEnvironment.h"

ChmEnvironment g_environment;

std::wstring ChmEnvironment::_getDefaultBasePath() {
	// デフォルト basePath = %appdata%\hitomoji\  となる。
	PWSTR path = nullptr;
	std::wstring retPath ;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path)))
	{
		retPath = path;
		CoTaskMemFree(path);
		retPath += L"\\hitomoji\\";
	}
	return retPath;
}
