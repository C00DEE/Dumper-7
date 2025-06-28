#pragma once

#include <unordered_set>
#include <unordered_map>
#include <iostream>
#include <format>
#include <functional>

#include "Unreal/Enums.h"


// 依赖管理器
class DependencyManager
{
public:
	using OnVisitCallbackType = std::function<void(int32 Index)>; // 访问回调类型

private:
	// 索引依赖信息
	struct IndexDependencyInfo
	{
		/* Counter incremented every time this element is hit during iteration, **if** the counter is less than the CurrentIterationIndex */
		/* 每次迭代期间命中此元素时递增的计数器，**如果**计数器小于CurrentIterationIndex */
		mutable uint64 IterationHitCounter = 0x0; // 迭代命中计数器

		/* Indices of Objects required by this Object */
		/* 此对象所需的对象索引 */
		std::unordered_set<int32> DependencyIndices; // 依赖项索引
	};

private:
	/* List of Objects and their Dependencies */
	/* 对象及其依赖项的列表 */
	std::unordered_map<int32, IndexDependencyInfo> AllDependencies;

	/* Count to track how often the Dependency-List was iterated. Allows for up to 2^64 iterations of this list. */
	/* 跟踪依赖关系列表被迭代次数的计数。允许此列表最多进行2^64次迭代。 */
	mutable uint64 CurrentIterationHitCount = 0x0; // 当前迭代命中计数

public:
	DependencyManager() = default;

	DependencyManager(int32 ObjectToTrack);

private:
	// 访问索引和依赖项
	void VisitIndexAndDependencies(int32 Index, OnVisitCallbackType Callback) const;

public:
	// 设置存在
	void SetExists(const int32 DepedantIdx);

	// 添加依赖项
	void AddDependency(const int32 DepedantIdx, int32 DependencyIndex);

	// 设置依赖项
	void SetDependencies(const int32 DepedantIdx, std::unordered_set<int32>&& Dependencies);

	// 获取条目数
	size_t GetNumEntries() const;

	// 使用回调访问索引和依赖项
	void VisitIndexAndDependenciesWithCallback(int32 Index, OnVisitCallbackType Callback) const;
	// 使用回调访问所有节点
	void VisitAllNodesWithCallback(OnVisitCallbackType Callback) const;

public:
	// 调试依赖关系图
	const auto DEBUG_DependencyMap() const
	{
		return AllDependencies;
	}
};
