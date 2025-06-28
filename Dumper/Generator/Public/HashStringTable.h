#pragma once

#include <cassert>
#include <format>
#include <iostream>

#include "Unreal/Enums.h"


#define WINDOWS_IGNORE_PACKING_MISMATCH // 宏定义，用于忽略Windows平台下的打包不匹配问题

// 哈希位数
inline constexpr uint32 NumHashBits = 5;
// 最大哈希数
inline constexpr uint32 MaxHashNumber = 1 << NumHashBits;
// 哈希掩码
inline constexpr uint32 HashMask = MaxHashNumber - 1;

// 5位Pearson哈希置换表
static constexpr uint8_t FiveBitPermutation[MaxHashNumber] = {
    0x20, 0x1D, 0x08, 0x18, 0x06, 0x0F, 0x15, 0x19,
    0x13, 0x1F, 0x17, 0x10, 0x14, 0x0C, 0x0E, 0x02,
    0x16, 0x11, 0x12, 0x0A, 0x0B, 0x1E, 0x04, 0x07,
    0x1B, 0x03, 0x0D, 0x1A, 0x1C, 0x01, 0x09, 0x05,
};

// 计算字符串的小型Pearson哈希值
inline uint8 SmallPearsonHash(const char* StringToHash)
{
    uint8 Hash = 0;

    while (*StringToHash != '\0')
    {
        const uint8 MaskedDownChar = (*StringToHash - 'A') & HashMask;
        Hash = FiveBitPermutation[Hash ^ MaskedDownChar];
        StringToHash++;
    }

    return (Hash & HashMask);
}

/* 用于限制对StringEntry::OptionalCollisionCount的访问，仅授权给友元类 */
struct AccessLimitedCollisionCount
{
    friend class PackageManager;
    friend class PackageInfoHandle;

private:
    uint8 CollisionCount; // 碰撞次数

public:
    AccessLimitedCollisionCount(uint8 Value)
        : CollisionCount(Value)
    {
    }
};

#pragma pack(push, 0x1)
// 字符串条目类
class StringEntry
{
private:
    friend class HashStringTable;
    friend class HashStringTableTest;

    template<typename CharType>
    friend int32 Strcmp(const CharType* String, const StringEntry& Entry);

public:
    // 字符串最大长度
    static constexpr int32 MaxStringLength = 1 << 11;

    // 不包含字符串的字符串条目大小
    static constexpr int32 StringEntrySizeWithoutStr = 0x3;

private:
    // 对象名称的长度
    uint16 Length : 11;

    // 缩减至5位的Pearson哈希值 --- 仅在 StringTable::CurrentMode == EDuplicationCheckingMode::Check 时添加字符串时计算
    uint16 Hash : 5;

    // 此字符串使用的是char还是wchar_t
    uint8 bIsWide : 1;

    // 此名称在其他名称中是否唯一
    mutable uint8 bIsUnique : 1;

    // 允许在不将此名称添加到表中的情况下检查其是否唯一
    mutable uint8 bIsUniqueTemp : 1;

    // 可选的碰撞计数，大多数情况下不使用。为在PackageManager中使用而实现
    mutable uint8 OptionalCollisionCount : 5;

    union
    {
        // NOT null-terminated
        char Char[MaxStringLength];
        wchar_t WChar[MaxStringLength];
    };

private:
    // 获取长度（以字节为单位）
    inline int32 GetLengthBytes() const { return StringEntrySizeWithoutStr + Length; }

    // 获取字符串长度
    inline int32 GetStringLength() const { return Length; }

public:
    // 在表中是否唯一
    inline bool IsUniqueInTable() const { return bIsUnique; }
    // 是否唯一
    inline bool IsUnique() const { return bIsUnique && bIsUniqueTemp; }

    // 获取哈希值
    inline uint8 GetHash() const { return Hash; }

    /* To be used with PackageManager */
    // 获取碰撞次数
    AccessLimitedCollisionCount GetCollisionCount() const { return { OptionalCollisionCount }; }

    // 获取名称
    inline std::string GetName() const { return std::string(Char, GetStringLength()); }
    // 获取宽字符名称
    inline std::wstring GetWideName() const { return std::wstring(WChar, GetStringLength()); }
    // 获取名称视图
    inline std::string_view GetNameView() const { return std::string_view(Char, GetStringLength()); }
    // 获取宽字符名称视图
    inline std::wstring_view GetWideNameView() const { return std::wstring_view(WChar, GetStringLength()); }
};
#pragma pack(pop)

// 字符串比较
template<typename CharType>
inline int32 Strcmp(const CharType* String, const StringEntry& Entry)
{
    static_assert(std::is_same_v<CharType, wchar_t> || std::is_same_v<CharType, char>);

    return memcmp(Entry.Char, String, Entry.GetStringLength() * sizeof(CharType));
}

// 哈希字符串表索引
struct HashStringTableIndex
{
public:
    // 无效索引
    static constexpr int32 InvalidIndex = -1;

public:
    uint32 Unused : 1; // 未使用
    uint32 HashIndex : 5; // 哈希索引
    uint32 InBucketOffset : 26; // 桶内偏移

public:
    inline HashStringTableIndex& operator=(uint32 Value)
    {
        *reinterpret_cast<uint32*>(this) = Value;

        return *this;
    }

    // 从整数创建索引
    static inline HashStringTableIndex FromInt(uint32 Idx)
    {
        return *reinterpret_cast<HashStringTableIndex*>(&Idx);
    }

    inline operator int32() const
    {
        return *reinterpret_cast<const int32*>(this);
    }

    explicit inline operator bool() const
    {
        return *this != InvalidIndex;
    }

    inline bool operator==(HashStringTableIndex Other) const { return static_cast<uint32>(*this) == static_cast<uint32>(Other); }
    inline bool operator!=(HashStringTableIndex Other) const { return static_cast<uint32>(*this) != static_cast<uint32>(Other); }

    inline bool operator==(int32 Other) const { return static_cast<int32>(*this) == Other; }
    inline bool operator!=(int32 Other) const { return static_cast<int32>(*this) != Other; }
};

// 哈希字符串表
class HashStringTable
{
private:
    static constexpr int64 NumHashBits = 5; // 哈希位数
    static constexpr int64 NumBuckets = 1 << NumHashBits; // 桶的数量

    /* Checked, Unchecked */
    static constexpr int64 NumSectionsPerBucket = 2; // 每个桶的分区数

private:
    // 字符串桶
    struct StringBucket
    {
        // 一个分配的块，分为两个部分，已检查和未检查
        uint8* Data;
        uint32 Size;
        uint32 SizeMax;
    };

private:
    StringBucket Buckets[NumBuckets]; // 桶数组

public:
    HashStringTable(uint32 InitialBucketSize = 0x5000);
    ~HashStringTable();

public:
    // 哈希桶迭代器
    class HashBucketIterator
    {
    private:
        const StringBucket* IteratedBucket; // 正在迭代的桶
        uint32 InBucketIndex; // 桶内索引

    public:
        HashBucketIterator(const StringBucket& Bucket, uint32 InBucketStartPos = 0)
            : IteratedBucket(&Bucket)
            , InBucketIndex(InBucketStartPos)
        {
        }

    public:
        static inline HashBucketIterator begin(const StringBucket& Bucket) { return HashBucketIterator(Bucket, 0); }
        static inline HashBucketIterator end(const StringBucket& Bucket) { return HashBucketIterator(Bucket, Bucket.Size); }

    public:
        inline uint32 GetInBucketIndex() const { return InBucketIndex; }
        inline const StringEntry& GetStringEntry() const { return *reinterpret_cast<StringEntry*>(IteratedBucket->Data + InBucketIndex); }

    public:
        inline bool operator==(const HashBucketIterator& Other) const { return InBucketIndex == Other.InBucketIndex; }
        inline bool operator!=(const HashBucketIterator& Other) const { return InBucketIndex != Other.InBucketIndex; }

        inline const StringEntry& operator*() const { return GetStringEntry(); }

        inline HashBucketIterator& operator++()
        {
            InBucketIndex += GetStringEntry().GetLengthBytes();

            return *this;
        }
    };

    // 哈希字符串表迭代器
    class HashStringTableIterator
    {
    private:
        const HashStringTable& IteratedTable; // 正在迭代的哈希表
        HashBucketIterator CurrentBucketIterator; // 当前桶迭代器
        uint32 BucketIdx; // 桶索引

    public:
        HashStringTableIterator(const HashStringTable& Table, uint32 BucketStartPos = 0, uint32 InBucketStartPos = 0)
            : IteratedTable(Table)
            , CurrentBucketIterator(Table.Buckets[BucketStartPos], InBucketStartPos)
            , BucketIdx(BucketStartPos)
        {
            while (CurrentBucketIterator == HashBucketIterator::end(IteratedTable.Buckets[BucketIdx]) && (++BucketIdx < NumBuckets))
                CurrentBucketIterator = HashBucketIterator::begin(IteratedTable.Buckets[BucketIdx]);
        }

        /* 仅用于 'end' */
        HashStringTableIterator(const HashStringTable& Table, HashBucketIterator LastBucketEndIterator)
            : IteratedTable(Table)
            , CurrentBucketIterator(LastBucketEndIterator)
            , BucketIdx(NumBuckets)
        {
        }

    public:
        inline uint32 GetBucketIndex() const { return BucketIdx; }
        inline uint32 GetInBucketIndex() const { return CurrentBucketIterator.GetInBucketIndex(); }

    public:
        inline bool operator==(const HashStringTableIterator& Other) const { return BucketIdx == Other.BucketIdx && CurrentBucketIterator == Other.CurrentBucketIterator; }
        inline bool operator!=(const HashStringTableIterator& Other) const { return BucketIdx != Other.BucketIdx || CurrentBucketIterator != Other.CurrentBucketIterator; }

        inline const StringEntry& operator*() const { return *CurrentBucketIterator; }

        inline HashStringTableIterator& operator++()
        {
            ++CurrentBucketIterator;

            while (CurrentBucketIterator == HashBucketIterator::end(IteratedTable.Buckets[BucketIdx]) && (++BucketIdx < NumBuckets))
                CurrentBucketIterator = HashBucketIterator::begin(IteratedTable.Buckets[BucketIdx]);

            return *this;
        }
    };

public:
    inline HashStringTableIterator begin() const { return HashStringTableIterator(*this, 0); }
    inline HashStringTableIterator end()   const { return HashStringTableIterator(*this, HashBucketIterator::end(Buckets[NumBuckets - 1])); }

private:
    // 检查桶是否能容纳指定长度的字符串
    bool CanFit(const StringBucket& Bucket, int32 StrLengthBytes) const;

    // 获取桶中空闲位置的引用
    StringEntry& GetRefToEmpty(const StringBucket& Bucket);
    // 获取桶中指定索引的字符串条目
    const StringEntry& GetStringEntry(const StringBucket& Bucket, int32 InBucketIndex) const;
    const StringEntry& GetStringEntry(int32 BucketIndex, int32 InBucketIndex) const;

    // 调整桶的大小
    void ResizeBucket(StringBucket& Bucket);

    // 添加未经检查的字符串
    template<typename CharType>
    std::pair<HashStringTableIndex, bool> AddUnchecked(const CharType* Str, int32 Length, uint8 Hash);

public:
    const StringEntry& operator[](HashStringTableIndex Index) const;

public:
    // 获取指定索引的桶
    const StringBucket& GetBucket(uint32 Index) const;
    // 获取指定索引的字符串条目
    const StringEntry& GetStringEntry(HashStringTableIndex Index) const;

    // 查找字符串
    template<typename CharType>
    HashStringTableIndex Find(const CharType* Str, int32 Length, uint8 Hash);

    // 查找或添加字符串
    template<typename CharType>
    std::pair<HashStringTableIndex, bool> FindOrAdd(const CharType* Str, int32 Length, bool bShouldMarkAsDuplicated = true);

    /* returns pair<Index, bWasAdded> */
    // 查找或添加字符串
    std::pair<HashStringTableIndex, bool> FindOrAdd(const std::string& String, bool bShouldMarkAsDuplicated = true);

    // 获取总使用大小
    int32 GetTotalUsedSize() const;

public:
    // 调试打印统计信息
    void DebugPrintStats() const;
};