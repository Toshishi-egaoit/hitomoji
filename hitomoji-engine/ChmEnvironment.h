#pragma once

#include <shlobj.h>

class ChmEnvironment {
public:
	void Init() {
		if (_initialiazed) {
			return;
		}
		else {
			_initialiazed = true;
			SetBasePath(_getDefaultBasePath() );
		}
	}

	void SetBasePath(std::wstring newPath) {
		_basePath = newPath ;
	};
	std::wstring GetBasePath() { return _basePath ; };

private:
	std::wstring _getDefaultBasePath() {
		// デフォルト basePath = Roaming\hitomoji\  となる。
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


	static std::wstring _basePath;
	bool _initialiazed = false;
};

extern ChmEnvironment g_environment;
