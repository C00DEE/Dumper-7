#include "Generators/Generator.h"
#include "Managers/StructManager.h"
#include "Managers/EnumManager.h"
#include "Managers/MemberManager.h"
#include "Managers/PackageManager.h"

#include "HashStringTable.h"
#include "Utils.h"

// 初始化弱对象指针设置
inline void InitWeakObjectPtrSettings()
{
	// 查找名为 "LoadAsset" 的UE函数，类型转换为函数
	UEStruct LoadAsset = ObjectArray::FindObjectFast<UEFunction>("LoadAsset", EClassCastFlags::Function);
	// 如果未找到 "LoadAsset" 函数
	if (!LoadAsset)
	{
		// 输出错误信息
		std::cerr << "\nDumper-7: 'LoadAsset' wasn't found, could not determine value for 'bIsWeakObjectPtrWithoutTag'!\n" << std::endl;
		return;
	}
	// 在 "LoadAsset" 函数中查找名为 "Asset" 的成员，类型转换为软对象属性
	UEProperty Asset = LoadAsset.FindMember("Asset", EClassCastFlags::SoftObjectProperty);
	// 如果未找到 "Asset" 成员
	if (!Asset)
	{
		// 输出错误信息
		std::cerr << "\nDumper-7: 'Asset' wasn't found, could not determine value for 'bIsWeakObjectPtrWithoutTag'!\n" << std::endl;
		return;
	}
	// 查找名为 "SoftObjectPath" 的UE结构体
	UEStruct SoftObjectPath = ObjectArray::FindObjectFast<UEStruct>("SoftObjectPath");
	// FWeakObjectPtr的大小固定为0x08
	constexpr int32 SizeOfFFWeakObjectPtr = 0x08;
	// 旧版Unreal资源指针的大小为0x10
	constexpr int32 OldUnrealAssetPtrSize = 0x10;
	// 获取SoftObjectPath结构体的大小，如果未找到则使用旧版资源指针大小
	const int32 SizeOfSoftObjectPath = SoftObjectPath ? SoftObjectPath.GetStructSize() : OldUnrealAssetPtrSize;
	// 设置内部变量，判断弱对象指针是否不带标签
	Settings::Internal::bIsWeakObjectPtrWithoutTag = Asset.GetSize() <= (SizeOfSoftObjectPath + SizeOfFFWeakObjectPtr);

	//std::cerr << std::format("\nDumper-7: bIsWeakObjectPtrWithoutTag = {}\n", Settings::Internal::bIsWeakObjectPtrWithoutTag) << std::endl;
}

// 初始化大型世界坐标设置
inline void InitLargeWorldCoordinateSettings()
{
	// 查找名为 "Vector" 的UE结构体，类型转换为结构体
	UEStruct FVectorStruct = ObjectArray::FindObjectFast<UEStruct>("Vector", EClassCastFlags::Struct);
	// 如果未找到FVector结构体
	if (!FVectorStruct) [[unlikely]]
	{
		// 输出严重错误信息
		std::cerr << "\nSomething went horribly wrong, FVector wasn't even found!\n\n" << std::endl;
		return;
	}
	// 在FVector结构体中查找名为 "X" 的成员
	UEProperty XProperty = FVectorStruct.FindMember("X");
	// 如果未找到 "X" 成员
	if (!XProperty) [[unlikely]]
	{
		// 输出严重错误信息
		std::cerr << "\nSomething went horribly wrong, FVector::X wasn't even found!\n\n" << std::endl;
		return;
	}

	/* 检查FVector::X的底层类型。如果是double，则我们处于UE5.0或更高版本，并使用大型世界坐标。 */
	Settings::Internal::bUseLargeWorldCoordinates = XProperty.IsA(EClassCastFlags::DoubleProperty);

	//std::cerr << std::format("\nDumper-7: bUseLargeWorldCoordinates = {}\n", Settings::Internal::bUseLargeWorldCoordinates) << std::endl;
}

// 初始化设置
inline void InitSettings()
{
	InitWeakObjectPtrSettings();
	InitLargeWorldCoordinateSettings();
}

// 初始化引擎核心
void Generator::InitEngineCore()
{
	/* 手动覆盖 */
	//ObjectArray::Init(/*GObjects*/, /*ChunkSize*/, /*bIsChunked*/);
	//FName::Init(/*FName::AppendString*/);
	//FName::Init(/*FName::ToString, FName::EOffsetOverrideType::ToString*/);
	//FName::Init(/*GNames, FName::EOffsetOverrideType::GNames, true/false*/);
	//Off::InSDK::ProcessEvent::InitPE(/*PEIndex*/);

	/* Back4Blood (需要手动覆盖GNames) */
	//InitObjectArrayDecryption([](void* ObjPtr) -> uint8* { return reinterpret_cast<uint8*>(uint64(ObjPtr) ^ 0x8375); });

	/* Multiversus [不支持, 奇怪的GObjects结构] */
	//InitObjectArrayDecryption([](void* ObjPtr) -> uint8* { return reinterpret_cast<uint8*>(uint64(ObjPtr) ^ 0x1B5DEAFD6B4068C); });
	// 初始化对象数组
	ObjectArray::Init();
	// 初始化FName
	FName::Init();
	// 初始化偏移量
	Off::Init();
	// 初始化属性大小
	PropertySizes::Init();
	// 初始化ProcessEvent的偏移量，必须在此位置，因为它依赖于Off::Init()中初始化的偏移量
	Off::InSDK::ProcessEvent::InitPE(); 

	// 初始化GWorld的偏移量，必须在此位置，因为它依赖于Off::Init()中初始化的偏移量
	Off::InSDK::World::InitGWorld(); 

	// 初始化文本偏移量，必须在此位置，因为它依赖于Off::InSDK::ProcessEvent::InitPE()中初始化的偏移量
	Off::InSDK::Text::InitTextOffsets(); 
	// 初始化设置
	InitSettings();
}

// 内部初始化
void Generator::InitInternal()
{
	// 使用所有包、它们的名称、结构体、类、枚举、函数和依赖项初始化PackageManager
	PackageManager::Init();

	// 使用所有结构体及其名称初始化StructManager
	StructManager::Init();
	
	// 使用所有枚举及其名称初始化EnumManager
	EnumManager::Init();
	
	// 初始化所有成员名称冲突
	MemberManager::Init();

	// 在StructManager初始化后对PackageManager进行后初始化。'PostInit()'处理循环依赖检测
	PackageManager::PostInit();
}

// 设置转储文件夹
bool Generator::SetupDumperFolder()
{
	try
	{
		// 根据游戏版本和名称创建文件夹名称
		std::string FolderName = (Settings::Generator::GameVersion + '-' + Settings::Generator::GameName);
		// 使文件夹名称有效
		FileNameHelper::MakeValidFileName(FolderName);
		// 设置转储文件夹路径
		DumperFolder = fs::path(Settings::Generator::SDKGenerationPath) / FolderName;
		// 如果转储文件夹已存在
		if (fs::exists(DumperFolder))
		{
			// 创建一个旧文件夹路径
			fs::path Old = DumperFolder.generic_string() + "_OLD";
			// 删除旧的备份文件夹
			fs::remove_all(Old);
			// 将当前文件夹重命名为旧文件夹
			fs::rename(DumperFolder, Old);
		}
		// 创建新的转储文件夹
		fs::create_directories(DumperFolder);
	}
	catch (const std::filesystem::filesystem_error& fe)
	{
		// 输出错误信息
		std::cerr << "Could not create required folders! Info: \n";
		std::cerr << fe.what() << std::endl;
		return false;
	}

	return true;
}

// 设置文件夹
bool Generator::SetupFolders(std::string& FolderName, fs::path& OutFolder)
{
	fs::path Dummy;
	std::string EmptyName = "";
	// 调用重载函数
	return SetupFolders(FolderName, OutFolder, EmptyName, Dummy);
}

// 设置文件夹
bool Generator::SetupFolders(std::string& FolderName, fs::path& OutFolder, std::string& SubfolderName, fs::path& OutSubFolder)
{
	// 使文件夹和子文件夹名称有效
	FileNameHelper::MakeValidFileName(FolderName);
	FileNameHelper::MakeValidFileName(SubfolderName);

	try
	{
		// 设置输出文件夹和子文件夹路径
		OutFolder = DumperFolder / FolderName;
		OutSubFolder = OutFolder / SubfolderName;
				
		// 如果输出文件夹已存在
		if (fs::exists(OutFolder))
		{
			// 创建一个旧文件夹路径
			fs::path Old = OutFolder.generic_string() + "_OLD";
			// 删除旧的备份文件夹
			fs::remove_all(Old);
			// 将当前文件夹重命名为旧文件夹
			fs::rename(OutFolder, Old);
		}
		// 创建新的输出文件夹
		fs::create_directories(OutFolder);
		// 如果子文件夹名称不为空，则创建子文件夹
		if (!SubfolderName.empty())
			fs::create_directories(OutSubFolder);
	}
	catch (const std::filesystem::filesystem_error& fe)
	{
		// 输出错误信息
		std::cerr << "Could not create required folders! Info: \n";
		std::cerr << fe.what() << std::endl;
		return false;
	}

	return true;
}