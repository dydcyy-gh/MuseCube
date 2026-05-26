#include "rng.h"
#include "variables.h"

// RNG初始化
void RNG_Init(void)
{
    RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);
    RNG_Cmd(ENABLE);
}

// 获取指定范围的真实随机数 (基于硬件RNG，每次都变)
uint32_t RNG_GetRandomRange(uint32_t min, uint32_t max)
{
	while(RNG_GetFlagStatus(RNG_FLAG_DRDY) == RESET);
    uint32_t random = RNG_GetRandomNumber();
    return min + (random % (max - min + 1));
}

// 新增：获取指定范围的固定随机数 (基于当前日期和范围，条件不变结果绝对不变)
uint32_t RNG_GetFixedRandomByDate(uint32_t min, uint32_t max)
{
    // 参数保护
    if (min > max) {
        uint32_t temp = min;
        min = max;
        max = temp;
    }

    // 1. 使用 FNV-1a 哈希算法作为基础，将所有关键参数混合
    uint32_t hash = 0x811C9DC5; // FNV 初始哈希值
    const uint32_t prime = 0x01000193; // FNV 素数

    // 混入日期信息
    hash = (hash ^ now_date.RTC_Year) * prime;
    hash = (hash ^ now_date.RTC_Month) * prime;
    hash = (hash ^ now_date.RTC_Date) * prime;
    
    // 混入范围信息 (防止同一天内不同范围抽出相同的原始hash)
    hash = (hash ^ min) * prime;
    hash = (hash ^ max) * prime;

    // 2. 使用 MurmurHash3 的 Finalizer 进一步打散数据，保证低位分布极度均匀
    hash ^= hash >> 16;
    hash *= 0x85ebca6b;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35;
    hash ^= hash >> 16;

    // 3. 映射到所需范围
    return min + (hash % (max - min + 1));
}
