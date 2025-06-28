#include "Managers/DependencyManager.h"

/*
* DependencyManager.cpp 实现了一个用于管理对象间依赖关系的系统。
*
* 在代码生成中，确定正确的定义顺序至关重要。例如，如果一个结构体B
* 包含了另一个结构体A的成员，那么A的定义必须出现在B之前。这个管理器
* 构建了一个依赖图，并提供了按正确顺序遍历这些依赖项的功能，以确保
* 生成的代码是可编译的。
*
* 它使用深度优先搜索（DFS）算法来遍历依赖关系，并通过一个迭代计数器来
* 处理循环依赖和避免重复访问。
*/


// 构造函数，初始化并追踪一个给定的对象索引。
DependencyManager::DependencyManager(int32 ObjectToTrack)
{
	AllDependencies.try_emplace(ObjectToTrack, IndexDependencyInfo{ });
}

// 确保一个依赖项索引存在于依赖映射中。
void DependencyManager::SetExists(const int32 DepedantIdx)
{
	AllDependencies[DepedantIdx];
}

// 为一个给定的对象（DepedantIdx）添加一个它所依赖的对象（DependencyIndex）。
void DependencyManager::AddDependency(const int32 DepedantIdx, int32 DependencyIndex)
{
	AllDependencies[DepedantIdx].DependencyIndices.insert(DependencyIndex);
}

// 直接为一个给定的对象（DepedantIdx）设置一整套依赖项。
void DependencyManager::SetDependencies(const int32 DepedantIdx, std::unordered_set<int32>&& Dependencies)
{
	AllDependencies[DepedantIdx].DependencyIndices = std::move(Dependencies);
}

// 获取当前管理器中追踪的条目总数。
size_t DependencyManager::GetNumEntries() const
{
	return AllDependencies.size();
}

// 内部递归函数，用于访问一个索引及其所有依赖项。
// 这是一个深度优先遍历（DFS）的实现。
// `CurrentIterationHitCount` 用于防止在单次遍历中重复访问同一个节点。
void DependencyManager::VisitIndexAndDependencies(int32 Index, OnVisitCallbackType Callback) const
{
	auto& [IterationHitCounter, Dependencies] = AllDependencies.at(Index);

	// 如果当前节点的迭代命中计数器已经达到当前遍历的计数值，说明它已被访问过。
	if (IterationHitCounter >= CurrentIterationHitCount)
		return;
	
	// 将当前节点的计数器更新为当前遍历的计数值，标记为已访问。
	IterationHitCounter = CurrentIterationHitCount;

	// 递归地访问所有依赖项。
	for (int32 Dependency : Dependencies)
	{
		VisitIndexAndDependencies(Dependency, Callback);
	}

	// 在所有依赖项都被访问后，对当前索引执行回调函数（后序遍历）。
	Callback(Index);
}

// 公开接口，用于从一个特定的索引开始，访问它及其所有依赖项。
// 每次调用都会增加全局迭代命中计数，以开始一次新的、干净的遍历。
void DependencyManager::VisitIndexAndDependenciesWithCallback(int32 Index, OnVisitCallbackType Callback) const
{
	CurrentIterationHitCount++;

	VisitIndexAndDependencies(Index, Callback);
}

// 访问管理器中所有的节点及其依赖项。
void DependencyManager::VisitAllNodesWithCallback(OnVisitCallbackType Callback) const
{
	CurrentIterationHitCount++;

	for (const auto& [Index, DependencyInfo] : AllDependencies)
	{
		VisitIndexAndDependencies(Index, Callback);
	}
}
