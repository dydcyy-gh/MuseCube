#include "lots_unit.h"
#include <stdio.h>
#include "lvgl.h"
#include "rng.h"  // 之前定义的随机数函数

// 预设5个签的路径
static const char* lot_paths[5] = {
    "0:/SYSTEM/LOT1.bin", // 大吉
    "0:/SYSTEM/LOT2.bin", // 中吉
    "0:/SYSTEM/LOT3.bin", // 小吉
    "0:/SYSTEM/LOT4.bin", // 小凶
    "0:/SYSTEM/LOT5.bin"  // 大凶
};

static lv_obj_t * lots_cont = NULL;
static lv_obj_t * img_lot = NULL;

// 逻辑控制变量
static uint32_t timer_cnt = 0;      
static uint32_t shuffle_times = 0;  // 已切换次数
static uint32_t max_shuffle = 20;   // 总计洗牌切换多少次后停止
static uint8_t  final_result = 0;   // 最终锁定的索引
static bool     is_finished = false;

void Create_Lots_Unit(void)
{
    // 1. 创建容器 (240x180)
    lots_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(lots_cont, 240, 180);
    lv_obj_center(lots_cont);
    
    // 清除容器样式，使其完全透明且无边框
    lv_obj_set_style_bg_opa(lots_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lots_cont, 0, 0);
    lv_obj_set_style_pad_all(lots_cont, 0, 0);
    lv_obj_clear_flag(lots_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 2. 创建图片组件 (180x180)
    img_lot = lv_img_create(lots_cont);
    lv_obj_set_size(img_lot, 180, 180);
    
    // 【修改：图片靠右放置】 
    // 容器宽240，图片宽180，靠右即 X偏移 = 60
    lv_obj_align(img_lot, LV_ALIGN_RIGHT_MID, 0, 0);
    
    // 3. 计算今日固定结果 (1-5 映射到 0-4 索引)
    final_result = RNG_GetFixedRandomByDate(1, 5) - 1;

    // 4. 初始化洗牌参数
    timer_cnt = 0;
    shuffle_times = 0;
    is_finished = false;
    
    // 设置一张初始图片（可选）
    lv_img_set_src(img_lot, lot_paths[RNG_GetRandomRange(0, 4)]);
}

// 每20ms调用一次
void Update_Lots_Unit(void)
{
    if (is_finished || !img_lot) return;

    timer_cnt++;

    // 设定切换间隔：
    // 前期快速切换（每120ms/6帧），后期逐渐变慢（增加仪式感）
    uint32_t current_interval = (shuffle_times < 8) ? 6 : (shuffle_times - 1);

    if (timer_cnt >= current_interval) {
        timer_cnt = 0;
        shuffle_times++;

        if (shuffle_times < max_shuffle) {
            // 【洗牌中】随机显示
            uint32_t rand_idx = RNG_GetRandomRange(0, 4);
            lv_img_set_src(img_lot, lot_paths[rand_idx]);
        } 
        else {
            // 【最后一步】显示今日固定的运势
            lv_img_set_src(img_lot, lot_paths[final_result]);
            is_finished = true;
        }
    }
}

void Remove_Lots_Unit(void)
{
    if (lots_cont != NULL) {
        lv_obj_del(lots_cont);
        lots_cont = NULL;
        img_lot = NULL;
    }
}
