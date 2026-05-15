#include "debug.h"
#include "flashdb.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "debug_unit.h"
#include "usbd_cdc_conf.h"
#include "malloc.h"
#include "variables.h"

void tsdb_printf(const char *format, ...)
{
    if (!tsdb.parent.init_ok) return; 
    if (__get_IPSR() != 0) return;
	
    char buf[128];
    va_list args;
    
    va_start(args, format);
    int len = vsnprintf(buf, sizeof(buf) - 2, format, args);
    va_end(args);

    if (len > 0) 
    {
        if (len >= sizeof(buf) - 2) len = sizeof(buf) - 3;
        
        for (int i = 0; i < len; i++) {
            if (buf[i] == '\r' || buf[i] == '\n') {
                buf[i] = ' ';
            }
        }
        
        buf[len++] = '\n';
        buf[len] = '\0'; 
        
        struct fdb_blob blob;
        fdb_tsl_append(&tsdb, fdb_blob_make(&blob, buf, len));
    }
}

typedef enum {
    TSDB_OUT_LVGL = 0,
    TSDB_OUT_USB
} tsdb_out_target_t;

// 用于存储单条日志的结构体
typedef struct {
    uint32_t timestamp;
    char text[128];
} log_record_t;

struct tsdb_read_ctx {
    int max_count;        // 请求获取的最大条数
    int current_count;    // 实际已经读取的条数
    log_record_t *records;// 临时缓存数组
};

// 将 Unix 时间戳转换为 HH:MM:SS 字符串（考虑时区偏移）
static void timestamp_to_hms(uint32_t timestamp, char *buf, size_t buf_len)
{
    uint32_t local_seconds = timestamp % 86400; 
    
    uint8_t hour = local_seconds / 3600;
    uint8_t minute = (local_seconds % 3600) / 60;
    uint8_t second = local_seconds % 60;
    snprintf(buf, buf_len, "%02d:%02d:%02d", hour, minute, second);
}

// 逆向遍历的回调：将数据存入缓冲，但不直接打印
static bool tsdb_read_cb(fdb_tsl_t tsl, void *arg)
{
    struct tsdb_read_ctx *ctx = (struct tsdb_read_ctx *)arg;
    
    if (ctx->current_count >= ctx->max_count) return true; // 读够了就停止

    struct fdb_blob blob;
    // 获取当前记录对应的缓冲指针
    log_record_t *record = &ctx->records[ctx->current_count];
    
    record->timestamp = tsl->time;
    memset(record->text, 0, sizeof(record->text));

    // 从 FlashDB 中读取日志正文
    fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, record->text, sizeof(record->text)));
    size_t read_len = fdb_blob_read((fdb_db_t)&tsdb, &blob);
    
    if (read_len > 0) {
        if (read_len >= sizeof(record->text)) read_len = sizeof(record->text) - 1;
        record->text[read_len] = '\0';

        // 清洗换行符
        for(int i = 0; i < read_len; i++) {
            if(record->text[i] == '\r' || record->text[i] == '\n') record->text[i] = ' ';
        }

        ctx->current_count++;
    } 
    return false; // 继续遍历下一条
}

// 统一的处理函数：获取并正序打印
static void tsdb_show_recent_forward(int num, tsdb_out_target_t target)
{
    if (!tsdb.parent.init_ok || num <= 0) return;

    // 使用 CCM 内存临时分配日志缓冲，50条约为 50 * 132 = 6600 Bytes
    log_record_t *records = (log_record_t *)malloc_ccm(sizeof(log_record_t) * num);
    if (records == NULL) {
        // 如果 CCM 内存分配失败，直接返回
        return; 
    }

    struct tsdb_read_ctx ctx = { .max_count = num, .current_count = 0, .records = records };
    
    // 逆向查询出最新的 current_count 条日志（此时 records[0] 是最新，records[n] 是最旧）
    fdb_tsl_iter_reverse(&tsdb, tsdb_read_cb, &ctx);

    // 反向遍历输出（实现正序输出：最旧的先输出，最新的最后输出）
    for (int i = ctx.current_count - 1; i >= 0; i--) {
        char time_str[9]; // "HH:MM:SS" + '\0'
        timestamp_to_hms(records[i].timestamp, time_str, sizeof(time_str));

        if (target == TSDB_OUT_LVGL) {
            lvgl_printf("[%s] %s\n", time_str, records[i].text);
            vTaskDelay(pdMS_TO_TICKS(5));
        } else if (target == TSDB_OUT_USB) {
            usb_printf("[%s] %s\r\n", time_str, records[i].text);
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    // 打印完毕，释放 CCM 内存
    free_ccm(records);
}

void tsdb_show_recent_on_lvgl(int num)
{
    tsdb_show_recent_forward(num, TSDB_OUT_LVGL);
}

void tsdb_show_recent_on_usb(int num)
{
    tsdb_show_recent_forward(num, TSDB_OUT_USB);
}
