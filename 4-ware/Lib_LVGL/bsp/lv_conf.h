/**
 * @file lv_conf.h
 * LVGL v8.2.0 配置文件
 */

/*
 * 将此文件复制为 `lv_conf.h`
 * 1. 直接放在 `lvgl` 文件夹旁边
 * 2. 或者放在其他位置，然后：
 *    - 定义 `LV_CONF_INCLUDE_SIMPLE`
 *    - 将路径添加为包含路径
 */

/* clang-format off */
#if 1 /*设置为"1"以启用内容*/

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   颜色设置
 *====================*/

/*颜色深度: 1 (每像素1字节), 8 (RGB332), 16 (RGB565), 32 (ARGB8888)*/
#define LV_COLOR_DEPTH 16

/*交换RGB565颜色的两个字节。如果显示器具有8位接口（例如SPI）则很有用*/
#define LV_COLOR_16_SWAP 0

/*启用更复杂的绘图例程来管理屏幕透明度。
 *可用于UI在另一个层之上的情况，例如OSD菜单或视频播放器。
 *需要 `LV_COLOR_DEPTH = 32` 颜色，并且屏幕的 `bg_opa` 应设置为非 LV_OPA_COVER 值*/
#define LV_COLOR_SCREEN_TRANSP 0

/* 调整颜色混合函数的舍入方式。GPU计算颜色混合的方式可能不同。
 * 0: 向下舍入, 64: 从x.75向上舍入, 128: 从一半向上舍入, 192: 从x.25向上舍入, 254: 向上舍入 */
#define LV_COLOR_MIX_ROUND_OFS (LV_COLOR_DEPTH == 32 ? 0: 128)

/*图像的像素如果具有此颜色（色度键控）则不会被绘制*/
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)         /*纯绿色*/

/*=========================
   内存设置
 *=========================*/

/*1: 使用自定义malloc/free, 0: 使用内置的 `lv_mem_alloc()` 和 `lv_mem_free()`*/
#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM == 0
    /*可用于 `lv_mem_alloc()` 的内存大小（字节）（>= 2kB）*/
    #define LV_MEM_SIZE (24U * 1024U)          /*[字节]*/

    /*为内存池设置一个地址，而不是将其分配为普通数组。也可以在外部SRAM中。*/
    #define LV_MEM_ADR   0x10000000            /*CCM内存池*/
    /*提供一个内存分配器来获取LVGL的内存池，而不是提供一个地址。例如 my_malloc*/
    #if LV_MEM_ADR == 0
        //#define LV_MEM_POOL_INCLUDE your_alloc_library  /* 如果使用外部分配器，取消注释 */
        //#define LV_MEM_POOL_ALLOC   your_alloc          /* 如果使用外部分配器，取消注释 */
    #endif

#else       /*LV_MEM_CUSTOM*/
    #define LV_MEM_CUSTOM_INCLUDE "malloc.h"   /*动态内存函数的头文件*/
    #define LV_MEM_CUSTOM_ALLOC   malloc_ccm
    #define LV_MEM_CUSTOM_FREE    free_ccm
    #define LV_MEM_CUSTOM_REALLOC realloc_ccm
#endif     /*LV_MEM_CUSTOM*/

/*在渲染和其他内部处理机制期间使用的中间内存缓冲区的数量。
 *如果没有足够的缓冲区，您将看到错误日志消息。 */
#define LV_MEM_BUF_MAX_NUM 16

/*使用标准的 `memcpy` 和 `memset` 代替LVGL自己的函数。（可能更快，也可能不快）。*/
#define LV_MEMCPY_MEMSET_STD 1

/*====================
   HAL 设置
 *====================*/

/*默认显示刷新周期。LVGL将以此周期时间重绘更改的区域*/
#define LV_DISP_DEF_REFR_PERIOD 20      /*[毫秒]*/

/*输入设备读取周期（毫秒）*/
#define LV_INDEV_DEF_READ_PERIOD 20     /*[毫秒]*/

/*使用自定义滴答源，告知经过的时间（毫秒）。
 *它消除了手动使用 `lv_tick_inc()` 更新滴答的需要*/
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "FreeRTOS.h"         /*系统时间函数的头文件*/
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (xTaskGetTickCount())    /*计算当前系统时间（毫秒）的表达式*/
#endif   /*LV_TICK_CUSTOM*/

/*默认每英寸点数。用于初始化默认大小，例如小部件大小、样式边距。
 *（不是很重要，您可以调整它以修改默认大小和间距）*/
#define LV_DPI_DEF 260     /*[像素/英寸]*/
/*=======================
 * 功能配置
 *=======================*/

/*-------------
 * 绘图
 *-----------*/

/*启用复杂绘图引擎。
 *需要绘制阴影、渐变、圆角、圆形、弧形、斜线、图像变换或任何遮罩*/
#define LV_DRAW_COMPLEX 1
#if LV_DRAW_COMPLEX != 0

    /*允许缓冲一些阴影计算。
    *LV_SHADOW_CACHE_SIZE 是要缓冲的最大阴影大小，其中阴影大小为 `shadow_width + radius`
    *缓存具有 LV_SHADOW_CACHE_SIZE^2 的RAM成本*/
    #define LV_SHADOW_CACHE_SIZE 0

    /* 设置最大缓存的圆形数据数量。
    * 保存1/4圆的周长用于抗锯齿
    * 每个圆圈使用 radius * 4 字节（最常用的半径被保存）
    * 0: 禁用缓存 */
    #define LV_CIRCLE_CACHE_SIZE 4
#endif /*LV_DRAW_COMPLEX*/

/*默认图像缓存大小。图像缓存保持图像打开状态。
 *如果仅使用内置图像格式，则缓存没有真正优势。（即，如果没有添加新的图像解码器）
 *对于复杂的图像解码器（例如PNG或JPG），缓存可以节省连续打开/解码图像的时间。
 *但是，打开的图像可能会消耗额外的RAM。
 *0: 禁用缓存*/
#define LV_IMG_CACHE_DEF_SIZE   0

/*每个渐变允许的停止点数量。增加此值以允许更多停止点。
 *每个额外的停止点增加 (sizeof(lv_color_t) + 1) 字节*/
#define LV_GRADIENT_MAX_STOPS       2

/*默认渐变缓冲区大小。
 *当LVGL计算渐变"映射"时，可以将它们保存到缓存中以避免再次计算。
 *LV_GRAD_CACHE_DEF_SIZE 以字节为单位设置此缓存的大小。
 *如果缓存太小，映射将仅在绘图需要时分配。
 *0 表示无缓存。*/
#define LV_GRAD_CACHE_DEF_SIZE      0

/*允许对渐变进行抖动（以在有限颜色深度的显示器上实现视觉上平滑的颜色渐变）
 *LV_DITHER_GRADIENT 意味着分配对象渲染表面的一行或两行以上
 *内存消耗的增加是 (32位 * 对象宽度) 加上如果使用误差扩散则再加 24位 * 对象宽度 */
#define LV_DITHER_GRADIENT      0
#if LV_DITHER_GRADIENT
    /*增加对误差扩散抖动的支持。
     *误差扩散抖动可以获得更好的视觉效果，但在绘图时意味着更多的CPU消耗和内存。
     *内存消耗的增加是 (24位 * 对象宽度)*/
    #define LV_DITHER_ERROR_DIFFUSION   0
#endif

/*为旋转分配的最大缓冲区大小。
 *仅在显示驱动程序中启用软件旋转时使用。*/
#define LV_DISP_ROT_MAX_BUF (10*1024)

/*-------------
 * GPU
 *-----------*/

/*使用STM32的DMA2D（又名Chrom Art）GPU*/
#define LV_USE_GPU_STM32_DMA2D 0
#if LV_USE_GPU_STM32_DMA2D
    /*必须定义以包含目标处理器的CMSIS头文件路径
    例如 "stm32f769xx.h" 或 "stm32f429xx.h"*/
    #define LV_GPU_DMA2D_CMSIS_INCLUDE
#endif

/*使用NXP的PXP GPU iMX RTxxx平台*/
#define LV_USE_GPU_NXP_PXP 0
#if LV_USE_GPU_NXP_PXP
    /*1: 为PXP添加默认的裸机和FreeRTOS中断处理例程 (lv_gpu_nxp_pxp_osa.c)
    *   并在lv_init()期间自动调用lv_gpu_nxp_pxp_init()。注意，为了使用FreeRTOS OSA，必须定义符号SDK_OS_FREE_RTOS，
    *   否则选择裸机实现。
    *0: 必须在lv_init()之前手动调用lv_gpu_nxp_pxp_init()
    */
    #define LV_USE_GPU_NXP_PXP_AUTO_INIT 0
#endif

/*使用NXP的VG-Lite GPU iMX RTxxx平台*/
#define LV_USE_GPU_NXP_VG_LITE 0

/*使用SDL渲染器API*/
#define LV_USE_GPU_SDL 0
#if LV_USE_GPU_SDL
    #define LV_GPU_SDL_INCLUDE_PATH <SDL2/SDL.h>
    /*纹理缓存大小，默认8MB*/
    #define LV_GPU_SDL_LRU_SIZE (1024 * 1024 * 8)
    /*用于遮罩绘制的自定义混合模式，如果需要与较旧的SDL2库链接，请禁用*/
    #define LV_GPU_SDL_CUSTOM_BLEND_MODE (SDL_VERSION_ATLEAST(2, 0, 6))
#endif

/*-------------
 * 日志记录
 *-----------*/

/*启用日志模块*/
#define LV_USE_LOG 0
#if LV_USE_LOG

    /*应添加的日志的重要性：
    *LV_LOG_LEVEL_TRACE       大量日志以提供详细信息
    *LV_LOG_LEVEL_INFO        记录重要事件
    *LV_LOG_LEVEL_WARN        记录如果发生不需要但未导致问题的情况
    *LV_LOG_LEVEL_ERROR       仅关键问题，当系统可能失败时
    *LV_LOG_LEVEL_USER        仅用户添加的日志
    *LV_LOG_LEVEL_NONE        不记录任何内容*/
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

    /*1: 使用 'printf' 打印日志；
    *0: 用户需要使用 `lv_log_register_print_cb()` 注册回调*/
    #define LV_LOG_PRINTF 0

    /*在产生大量日志的模块中启用/禁用 LV_LOG_TRACE*/
    #define LV_LOG_TRACE_MEM        1
    #define LV_LOG_TRACE_TIMER      1
    #define LV_LOG_TRACE_INDEV      1
    #define LV_LOG_TRACE_DISP_REFR  1
    #define LV_LOG_TRACE_EVENT      1
    #define LV_LOG_TRACE_OBJ_CREATE 1
    #define LV_LOG_TRACE_LAYOUT     1
    #define LV_LOG_TRACE_ANIM       1

#endif  /*LV_USE_LOG*/

/*-------------
 * 断言
 *-----------*/

/*如果操作失败或发现无效数据，则启用断言。
 *如果启用了 LV_USE_LOG，则将在失败时打印错误消息*/
#define LV_USE_ASSERT_NULL          1   /*检查参数是否为NULL。（非常快，推荐）*/
#define LV_USE_ASSERT_MALLOC        1   /*检查内存是否成功分配。（非常快，推荐）*/
#define LV_USE_ASSERT_STYLE         0   /*检查样式是否正确初始化。（非常快，推荐）*/
#define LV_USE_ASSERT_MEM_INTEGRITY 0   /*在关键操作后检查 `lv_mem` 的完整性。（慢）*/
#define LV_USE_ASSERT_OBJ           0   /*检查对象的类型和存在性（例如未删除）。（慢）*/

/*当断言发生时添加自定义处理程序，例如重启MCU*/
#define LV_ASSERT_HANDLER_INCLUDE <stdint.h>
#define LV_ASSERT_HANDLER while(1);   /*默认情况下暂停*/

/*-------------
 * 其他
 *-----------*/

/*1: 显示CPU使用率和FPS计数*/
#define LV_USE_PERF_MONITOR 0
#if LV_USE_PERF_MONITOR
    #define LV_USE_PERF_MONITOR_POS LV_ALIGN_TOP_LEFT
#endif

/*1: 显示已用内存和内存碎片
 * 需要 LV_MEM_CUSTOM = 0*/
#define LV_USE_MEM_MONITOR 0
#if LV_USE_MEM_MONITOR
    #define LV_USE_MEM_MONITOR_POS LV_ALIGN_BOTTOM_LEFT
#endif

/*1: 在重绘区域上绘制随机颜色的矩形*/
#define LV_USE_REFR_DEBUG 0

/*更改内置的 (v)snprintf 函数*/
#define LV_SPRINTF_CUSTOM 0
#if LV_SPRINTF_CUSTOM
    #define LV_SPRINTF_INCLUDE <stdio.h>
    #define lv_snprintf  snprintf
    #define lv_vsnprintf vsnprintf
#else   /*LV_SPRINTF_CUSTOM*/
    #define LV_SPRINTF_USE_FLOAT 1
#endif  /*LV_SPRINTF_CUSTOM*/

#define LV_USE_USER_DATA 1

/*垃圾回收器设置
 *如果lvgl绑定到高级语言并且内存由该语言管理，则使用*/
#define LV_ENABLE_GC 0
#if LV_ENABLE_GC != 0
    #define LV_GC_INCLUDE "gc.h"                           /*包含垃圾回收器相关的内容*/
#endif /*LV_ENABLE_GC*/

/*=====================
 *  编译器设置
 *====================*/

/*对于大端系统设置为1*/
#define LV_BIG_ENDIAN_SYSTEM 0

/*为 `lv_tick_inc` 函数定义自定义属性*/
#define LV_ATTRIBUTE_TICK_INC

/*为 `lv_timer_handler` 函数定义自定义属性*/
#define LV_ATTRIBUTE_TIMER_HANDLER

/*为 `lv_disp_flush_ready` 函数定义自定义属性*/
#define LV_ATTRIBUTE_FLUSH_READY

/*缓冲区所需的对齐大小*/
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE 1

/*将在需要对齐内存的地方添加（使用-Os时数据可能默认不对齐到边界）。
 * 例如 __attribute__((aligned(4)))*/
#define LV_ATTRIBUTE_MEM_ALIGN

/*用于标记大型常量数组的属性，例如字体的位图*/
#define LV_ATTRIBUTE_LARGE_CONST

/*在RAM中声明大数组的编译器前缀*/
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY

/*将性能关键函数放入更快的内存中（例如RAM）*/
#define LV_ATTRIBUTE_FAST_MEM

/*前缀用于GPU加速操作的变量，这些变量通常需要放置在DMA可访问的RAM段中*/
#define LV_ATTRIBUTE_DMA

/*将整型常量导出到绑定。此宏与形式为 LV_<CONST> 的常量一起使用，
 *这些常量也应出现在LVGL绑定API上，例如Micropython。*/
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning /*默认值只是防止GCC警告*/

/*通过使用int32_t代替int16_t作为坐标，将默认的-32k..32k坐标范围扩展到-4M..4M*/
#define LV_USE_LARGE_COORD 0

/*==================
 *   字体使用
 *===================*/

/*Montserrat字体，ASCII范围和一些符号，使用bpp = 4
 *https://fonts.google.com/specimen/Montserrat*/
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

/*演示特殊功能*/
#define LV_FONT_MONTSERRAT_12_SUBPX      0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0  /*bpp = 3*/
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0  /*希伯来语、阿拉伯语、波斯语字母及其所有形式*/
#define LV_FONT_SIMSUN_16_CJK            0  /*1000个最常见的CJK部首*/

/*像素完美的等宽字体*/
#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0

/*可选地在此处声明自定义字体。
 *您也可以将这些字体用作默认字体，它们将在全局范围内可用。
 *例如 #define LV_FONT_CUSTOM_DECLARE   LV_FONT_DECLARE(my_font_1) LV_FONT_DECLARE(my_font_2)*/
#define LV_FONT_CUSTOM_DECLARE  	 \
		LV_FONT_DECLARE(Font8)  	 \
		LV_FONT_DECLARE(Font12) 	 \
		LV_FONT_DECLARE(Font16) 	 \
		LV_FONT_DECLARE(Font24) 	 \
		LV_FONT_DECLARE(lv_font_8)   \
		LV_FONT_DECLARE(lv_font_12)  \
		LV_FONT_DECLARE(lv_font_16)  \
		LV_FONT_DECLARE(lv_font_24)  \
		LV_FONT_DECLARE(lv_font_36)  \
		LV_FONT_DECLARE(lv_font_40)  \
		LV_FONT_DECLARE(lv_font_56)  \
		LV_FONT_DECLARE(lv_font_ipa) \

/*始终设置默认字体*/
#define LV_FONT_DEFAULT &lv_font_12

/*启用处理大字体和/或具有大量字符的字体。
 *限制取决于字体大小、字体面和bpp。
 *如果字体需要，将触发编译器错误。*/
#define LV_FONT_FMT_TXT_LARGE 0

/*启用/禁用对压缩字体的支持。*/
#define LV_USE_FONT_COMPRESSED 0

/*启用子像素渲染*/
#define LV_USE_FONT_SUBPX 0
#if LV_USE_FONT_SUBPX
    /*设置显示器的像素顺序。RGB通道的物理顺序。对于"普通"字体无关紧要。*/
    #define LV_FONT_SUBPX_BGR 0  /*0: RGB; 1:BGR 顺序*/
#endif

/*=================
 *  文本设置
 *=================*/

/**
 * 为字符串选择字符编码。
 * 您的IDE或编辑器应具有相同的字符编码
 * - LV_TXT_ENC_UTF8
 * - LV_TXT_ENC_ASCII
 */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/*可以在这些字符上打断（换行）文本*/
#define LV_TXT_BREAK_CHARS " ,.;:-_"

/*如果一个单词至少有这么长，将在"最漂亮"的地方换行
 *要禁用，设置为 <= 0 的值*/
#define LV_TXT_LINE_BREAK_LONG_LEN 0

/*在换行前放在一行上的长单词的最小字符数。
 *取决于 LV_TXT_LINE_BREAK_LONG_LEN。*/
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3

/*在换行后放在一行上的长单词的最小字符数。
 *取决于 LV_TXT_LINE_BREAK_LONG_LEN。*/
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3

/*用于信号文本重新着色的控制字符。*/
#define LV_TXT_COLOR_CMD "#"

/*支持双向文本。允许混合从左到右和从右到左的文本。
 *方向将根据Unicode双向算法处理：
 *https://www.w3.org/International/articles/inline-bidi-markup/uba-basics*/
#define LV_USE_BIDI 0
#if LV_USE_BIDI
    /*设置默认方向。支持的值：
    *`LV_BASE_DIR_LTR` 从左到右
    *`LV_BASE_DIR_RTL` 从右到左
    *`LV_BASE_DIR_AUTO` 检测文本基础方向*/
    #define LV_BIDI_BASE_DIR_DEF LV_BASE_DIR_AUTO
#endif

/*启用阿拉伯语/波斯语处理
 *在这些语言中，字符应根据其在文本中的位置替换为其他形式*/
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*==================
 *  小部件使用
 *================*/

/*小部件文档：https://docs.lvgl.io/latest/en/html/widgets/index.html*/

#define LV_USE_ARC        1

#define LV_USE_ANIMIMG    1

#define LV_USE_BAR        1

#define LV_USE_BTN        1

#define LV_USE_BTNMATRIX  1

#define LV_USE_CANVAS     1

#define LV_USE_CHECKBOX   1

#define LV_USE_DROPDOWN   1   /*需要: lv_label*/

#define LV_USE_IMG        1   /*需要: lv_label*/

#define LV_USE_LABEL      1
#if LV_USE_LABEL
    #define LV_LABEL_TEXT_SELECTION 1 /*启用选择标签文本*/
    #define LV_LABEL_LONG_TXT_HINT 1  /*在标签中存储一些额外信息以加速绘制非常长的文本*/
#endif

#define LV_USE_LINE       1

#define LV_USE_ROLLER     1   /*需要: lv_label*/
#if LV_USE_ROLLER
    #define LV_ROLLER_INF_PAGES 7 /*当滚动条无限时的额外"页面"数量*/
#endif

#define LV_USE_SLIDER     1   /*需要: lv_bar*/

#define LV_USE_SWITCH     1

#define LV_USE_TEXTAREA   1   /*需要: lv_label*/
#if LV_USE_TEXTAREA != 0
    #define LV_TEXTAREA_DEF_PWD_SHOW_TIME 1500    /*毫秒*/
#endif

#define LV_USE_TABLE      1

/*==================
 * 额外组件
 *==================*/

/*-----------
 * 小部件
 *----------*/
#define LV_USE_CALENDAR   1
#if LV_USE_CALENDAR
    #define LV_CALENDAR_WEEK_STARTS_MONDAY 0
	#if LV_CALENDAR_WEEK_STARTS_MONDAY
	#define LV_CALENDAR_DEFAULT_DAY_NAMES {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"}
	#else
	#define LV_CALENDAR_DEFAULT_DAY_NAMES {"日", "一", "二", "三", "四", "五", "六"}
	#endif
	
	#define LV_CALENDAR_DEFAULT_MONTH_NAMES {"January", "February", "March",  "April", "May",  "June", "July", "August", "September", "October", "November", "December"}
	#define LV_USE_CALENDAR_HEADER_ARROW 1
	#define LV_USE_CALENDAR_HEADER_DROPDOWN 1
#endif  /*LV_USE_CALENDAR*/

#define LV_USE_CHART      1

#define LV_USE_COLORWHEEL 1

#define LV_USE_IMGBTN     1

#define LV_USE_KEYBOARD   1

#define LV_USE_LED        1

#define LV_USE_LIST       1

#define LV_USE_MENU       1

#define LV_USE_METER      1

#define LV_USE_MSGBOX     1

#define LV_USE_SPINBOX    1

#define LV_USE_SPINNER    1

#define LV_USE_TABVIEW    1

#define LV_USE_TILEVIEW   1

#define LV_USE_WIN        1

#define LV_USE_SPAN       1
#if LV_USE_SPAN
    /*一行文本可以包含的最大跨度描述符数量 */
    #define LV_SPAN_SNIPPET_STACK_SIZE 64
#endif

/*-----------
 * 主题
 *----------*/

/*一个简单、令人印象深刻且非常完整的主题*/
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT

    /*0: 浅色模式; 1: 深色模式*/
    #define LV_THEME_DEFAULT_DARK 0

    /*1: 启用按下时增长*/
    #define LV_THEME_DEFAULT_GROW 1

    /*默认过渡时间 [毫秒]*/
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif /*LV_USE_THEME_DEFAULT*/

/*一个非常简单的主题，是自定义主题的良好起点*/
#define LV_USE_THEME_BASIC 1

/*为单色显示器设计的主题*/
#define LV_USE_THEME_MONO 1

/*-----------
 * 布局
 *----------*/

/*类似于CSS中的Flexbox的布局。*/
#define LV_USE_FLEX 1

/*类似于CSS中的Grid的布局。*/
#define LV_USE_GRID 1

/*---------------------
 * 第三方库
 *--------------------*/

/*常见API的文件系统接口 */

/*用于fopen, fread等的API*/
#define LV_USE_FS_STDIO 0
#if LV_USE_FS_STDIO
    #define LV_FS_STDIO_LETTER '\0'     /*设置一个大写字母，驱动器将通过该字母访问（例如 'A'）*/
    #define LV_FS_STDIO_PATH ""         /*设置工作目录。文件/目录路径将附加到其后。*/
    #define LV_FS_STDIO_CACHE_SIZE  0   /*>0 以在lv_fs_read()中缓存此字节数*/
#endif

/*用于open, read等的API*/
#define LV_USE_FS_POSIX 0
#if LV_USE_FS_POSIX
    #define LV_FS_POSIX_LETTER '\0'     /*设置一个大写字母，驱动器将通过该字母访问（例如 'A'）*/
    #define LV_FS_POSIX_PATH ""         /*设置工作目录。文件/目录路径将附加到其后。*/
    #define LV_FS_POSIX_CACHE_SIZE  0   /*>0 以在lv_fs_read()中缓存此字节数*/
#endif

/*用于CreateFile, ReadFile等的API*/
#define LV_USE_FS_WIN32 0
#if LV_USE_FS_WIN32
    #define LV_FS_WIN32_LETTER  '\0'    /*设置一个大写字母，驱动器将通过该字母访问（例如 'A'）*/
    #define LV_FS_WIN32_PATH ""         /*设置工作目录。文件/目录路径将附加到其后。*/
    #define LV_FS_WIN32_CACHE_SIZE 0    /*>0 以在lv_fs_read()中缓存此字节数*/
#endif

/*用于FATFS的API（需要单独添加）。使用f_open, f_read等*/
#define LV_USE_FS_FATFS  1
#if LV_USE_FS_FATFS
    #define LV_FS_FATFS_LETTER '0'     /*设置一个大写字母，驱动器将通过该字母访问（例如 'A'）*/
    #define LV_FS_FATFS_CACHE_SIZE 0    /*>0 以在lv_fs_read()中缓存此字节数*/
#endif

/*PNG解码库*/
#define LV_USE_PNG 0

/*BMP解码库*/
#define LV_USE_BMP 1

/* JPG + 分割JPG解码库。
 * 分割JPG是一种为嵌入式系统优化的自定义格式。 */
#define LV_USE_SJPG 0

/*GIF解码库*/
#define LV_USE_GIF 0

/*二维码库*/
#define LV_USE_QRCODE 0

/*FreeType库*/
#define LV_USE_FREETYPE 0
#if LV_USE_FREETYPE
    /*FreeType用于缓存字符的内存[字节] (-1: 无缓存)*/
    #define LV_FREETYPE_CACHE_SIZE (16 * 1024)
    #if LV_FREETYPE_CACHE_SIZE >= 0
        /* 1: 位图缓存使用sbit缓存, 0:位图缓存使用图像缓存。 */
        /* sbit缓存：对于小位图（字体大小 < 256）更节省内存 */
        /* 如果字体大小 >= 256，必须配置为图像缓存 */
        #define LV_FREETYPE_SBIT_CACHE 0
        /* 由此缓存实例管理的已打开FT_Face/FT_Size对象的最大数量。 */
        /* (0:使用系统默认值) */
        #define LV_FREETYPE_CACHE_FT_FACES 0
        #define LV_FREETYPE_CACHE_FT_SIZES 0
    #endif
#endif

/*Rlottie库*/
#define LV_USE_RLOTTIE 0

/*用于图像解码和播放视频的FFmpeg库
 *支持所有主要图像格式，因此不要与其他图像解码器一起启用它*/
#define LV_USE_FFMPEG  0
#if LV_USE_FFMPEG
    /*将输入信息转储到stderr*/
    #define LV_FFMPEG_AV_DUMP_FORMAT 0
#endif

/*-----------
 * 其他
 *----------*/

/*1: 启用为对象拍摄快照的API*/
#define LV_USE_SNAPSHOT 0

/*1: 启用Monkey测试*/
#define LV_USE_MONKEY   0

/*1: 启用网格导航*/
#define LV_USE_GRIDNAV  0

/*==================
* 示例
*==================*/

/*启用与库一起构建的示例*/
#define LV_BUILD_EXAMPLES 1

/*===================
 * 演示使用
 ====================*/

/*显示一些小部件。可能需要增加 `LV_MEM_SIZE` */
#define LV_USE_DEMO_WIDGETS        0
#if LV_USE_DEMO_WIDGETS
#define LV_DEMO_WIDGETS_SLIDESHOW  0
#endif

/*演示编码器和键盘的用法*/
#define LV_USE_DEMO_KEYPAD_AND_ENCODER     0

/*对系统进行基准测试*/
#define LV_USE_DEMO_BENCHMARK   0

/*LVGL的压力测试*/
#define LV_USE_DEMO_STRESS      0

/*MUSIC播放器演示*/
#define LV_USE_DEMO_MUSIC       0
#if LV_USE_DEMO_MUSIC
# define LV_DEMO_MUSIC_SQUARE       0
# define LV_DEMO_MUSIC_LANDSCAPE    0
# define LV_DEMO_MUSIC_ROUND        0
# define LV_DEMO_MUSIC_LARGE        0
# define LV_DEMO_MUSIC_AUTO_PLAY    0
#endif

/*--LV_CONF_H结束--*/

#endif /*LV_CONF_H*/

#endif
