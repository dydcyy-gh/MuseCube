#include "fal.h"
#include "w25q128.h" 
#include <string.h>
#include "flashdb.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "variables.h"
#include <time.h>

/* ================== 1. Flash 操作函数 (保持不变) ================== */
static int w25q_init(void) {
    return 0;
}

static int w25q_read(long offset, uint8_t *buf, size_t size) {
    W25QXX_Read(buf, offset, size);
    return 0;
}

static int w25q_write(long offset, const uint8_t *buf, size_t size) {
    W25QXX_Write_Raw((uint8_t *)buf, offset, size); 
    return 0;
}

static int w25q_erase(long offset, size_t size) {
    uint32_t addr = offset;
    size_t remain = size;

    while (remain > 0) {
        W25QXX_Erase_Sector(addr);
        addr += 4096;
        if (remain >= 4096) remain -= 4096;
        else remain = 0;
    }
    return 0;
}

/* ================== 2. 定义 Flash 设备对象 ================== */
static const struct fal_flash_dev w25q128_dev = {
    .name = "W25Q128",
    .addr = 0,
    .len = 8 * 1024 * 1024, 
    .blk_size = 4096,
    .ops = {w25q_init, w25q_read, w25q_write, w25q_erase},
    .write_gran = 1, 
};

/* ================== 3. 定义分区对象 ================== */

/* 分区1: KVDB (假设在前 4MB) */
static const struct fal_partition kv_part = {
    .magic_word = 0x45503130, 
    .name = "fdb_kvdb1",
    .flash_name = "W25Q128",
    .offset = 0,             
    .len = 4 * 1024 * 1024, /* 4MB */
    .reserved = 0,
};

/* 分区2: TSDB (4-8MB) */
static const struct fal_partition ts_part = {
    .magic_word = 0x45503130, 
    .name = "fdb_tsdb1",      /* FlashDB TSDB 默认查找的名字 */
    .flash_name = "W25Q128",
    .offset = 4 * 1024 * 1024, /* 偏移量：4MB 处开始 */
    .len = 4 * 1024 * 1024,    /* 长度：4MB */
    .reserved = 0,
};

/* ================== 4. 实现 FAL 接口函数 ================== */

int fal_init(void) 
{
    return 0;
}

const struct fal_partition *fal_partition_find(const char *name) {
    if (strcmp(name, "fdb_kvdb1") == 0) {
        return &kv_part;
    }

    if (strcmp(name, "fdb_tsdb1") == 0) {
        return &ts_part;
    }
    return NULL;
}

const struct fal_flash_dev *fal_flash_device_find(const char *name) {
    if (strcmp(name, "W25Q128") == 0) {
        return &w25q128_dev;
    }
    return NULL;
}

/* 转发函数保持不变 */
int fal_partition_read(const struct fal_partition *part, uint32_t addr, uint8_t *buf, size_t size) {
    return w25q_read(part->offset + addr, buf, size);
}

int fal_partition_write(const struct fal_partition *part, uint32_t addr, const uint8_t *buf, size_t size) {
    return w25q_write(part->offset + addr, buf, size);
}

int fal_partition_erase(const struct fal_partition *part, uint32_t addr, size_t size) {
    return w25q_erase(part->offset + addr, size);
}

/* ================== 4. 实现 FDB 互斥锁函数 ================== */
extern SemaphoreHandle_t xFDBSemaphore;//FDB信号量

void fdb_lock(fdb_db_t db)
{
    xSemaphoreTake(xFDBSemaphore, portMAX_DELAY);
    (void)db;
}

void fdb_unlock(fdb_db_t db)
{
    xSemaphoreGive(xFDBSemaphore);
    (void)db;
}

fdb_time_t get_fdb_time(void)
{
    struct tm tm_now;

    memset(&tm_now, 0, sizeof(struct tm)); 
    tm_now.tm_isdst = 0;

    tm_now.tm_year = now_date.RTC_Year + 100;
    tm_now.tm_mon  = now_date.RTC_Month - 1;
    tm_now.tm_mday = now_date.RTC_Date;
    tm_now.tm_hour = now_time.RTC_Hours;
    tm_now.tm_min  = now_time.RTC_Minutes;
    tm_now.tm_sec  = now_time.RTC_Seconds;
    
    return (fdb_time_t)mktime(&tm_now);
}
