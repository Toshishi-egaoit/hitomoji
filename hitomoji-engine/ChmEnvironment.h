#pragma once
#include <string>
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
	std::wstring _getDefaultBasePath() ;

	std::wstring _basePath;
	bool _initialiazed = false;
};

extern ChmEnvironment g_environment;
