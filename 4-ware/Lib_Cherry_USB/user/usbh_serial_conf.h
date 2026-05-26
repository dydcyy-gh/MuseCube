#ifndef __USBH_SERIAL_CONF_H__
#define __USBH_SERIAL_CONF_H__

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx.h"

// 保持原有的给 usb_task.c 调用的基础接口
void usbh_serial_init(uint8_t busid, uint32_t reg_base);
void usbh_serial_deinit(void);
void usbh_serial_task(void);

/* ======================================================= */
/*           新增：供“串口助手”应用层调用的 API 接口           */
/* ======================================================= */

// 获取当前是否有 USB 串口设备插入并准备就绪
bool app_usb_serial_is_connected(void);

// 打开串口设备 (供设备插入后，或者UI点击“打开串口”时调用)
int app_usb_serial_open(void);

// 关闭串口设备
void app_usb_serial_close(void);

// 配置串口参数 (波特率, 数据位, 停止位, 校验位)
// data_bits: 5/6/7/8
// parity: 0(None), 1(Odd), 2(Even), 3(Mark), 4(Space)
// stop_bits: 0(1 bit), 1(1.5 bits), 2(2 bits)
int app_usb_serial_config(uint32_t baudrate, uint8_t data_bits, uint8_t parity, uint8_t stop_bits);

// 发送数据 (非阻塞或带超时的阻塞)
int app_usb_serial_send(const uint8_t *data, uint32_t len);

// 接收数据 (非阻塞读取，UI定时器可周期性调用此函数提取数据)
int app_usb_serial_recv(uint8_t *buffer, uint32_t max_len);

#endif /* __USBH_SERIAL_CONF_H__ */
