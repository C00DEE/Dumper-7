#include "Settings.h"
#include <Windows.h>
#include <filesystem>
#include <string>

void Settings::Config::Load()
{
	namespace fs = std::filesystem;

	// 尝试本地的 Dumper-7.ini
	const std::string LocalPath = (fs::current_path() / "Dumper-7.ini").string();
	const char* ConfigPath = nullptr;

	if (fs::exists(LocalPath)) 
	{
		ConfigPath = LocalPath.c_str();
	}
	else if (fs::exists(GlobalConfigPath)) // 尝试全局路径
	{
		ConfigPath = GlobalConfigPath;
	}

	// 如果没有找到配置文件，则使用默认值
	if (!ConfigPath) 
		return;

	char SDKNamespace[256] = {};
	GetPrivateProfileStringA("Settings", "SDKNamespaceName", "SDK", SDKNamespace, sizeof(SDKNamespace), ConfigPath);

	SDKNamespaceName = SDKNamespace;
	SleepTimeout = max(GetPrivateProfileIntA("Settings", "SleepTimeout", 0, ConfigPath), 0);
}
