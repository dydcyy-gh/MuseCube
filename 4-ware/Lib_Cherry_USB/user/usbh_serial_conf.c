/*
 * usbh_serial_conf.c
 */

#include "FreeRTOS.h"
#include "task.h"
#include "usbh_core.h"
#include "usbh_serial.h"
#include "systick_conf.h"  // 根据你工程实际的延时头文件调整

static volatile uint8_t g_usbh_serial_connected = 0;
static volatile uint8_t g_usbh_serial_tested = 0;
static struct usbh_serial *g_serial_dev = NULL;

#define SERIAL_TEST_LEN 64

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t serial_tx_buffer[SERIAL_TEST_LEN];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t serial_rx_data[SERIAL_TEST_LEN];

int usb_serial_test(void)
{
    int ret;
    struct usbh_serial *serial = NULL;
    bool serial_test_success = false;
    uint32_t serial_tx_bytes = 0;
    uint32_t serial_rx_bytes = 0;

    // 1. 尝试打开串口设备
    serial = usbh_serial_open("/dev/ttyACM0", USBH_SERIAL_O_RDWR | USBH_SERIAL_O_NONBLOCK);
    if (serial == NULL) {
        serial = usbh_serial_open("/dev/ttyUSB0", USBH_SERIAL_O_RDWR | USBH_SERIAL_O_NONBLOCK);
        if (serial == NULL) {
            USB_LOG_RAW("no serial device found\r\n");
            return -1;
        }
    }

    // 2. 配置串口参数 115200 8N1
    struct usbh_serial_termios termios;
    memset(&termios, 0, sizeof(termios));
    termios.baudrate = 115200;
    termios.stopbits = 0;
    termios.parity = 0;
    termios.databits = 8;
    termios.rtscts = false;
    termios.rx_timeout = 0;
    ret = usbh_serial_control(serial, USBH_SERIAL_CMD_SET_ATTR, &termios);
    if (ret < 0) {
        USB_LOG_RAW("set serial attr error, ret:%d\r\n", ret);
        return -1;
    }

    // 3. 准备发送数据
    memset(serial_tx_buffer, 0xA5, sizeof(serial_tx_buffer));
    USB_LOG_RAW("start serial loopback test, len: %d\r\n", SERIAL_TEST_LEN);

    // 发送数据
    while (1) {
        ret = usbh_serial_write(serial, serial_tx_buffer, sizeof(serial_tx_buffer));
        if (ret < 0) {
            USB_LOG_RAW("serial write error, ret:%d\r\n", ret);
            return -1;
        } else {
            serial_tx_bytes += ret;
            if (serial_tx_bytes == SERIAL_TEST_LEN) {
                USB_LOG_RAW("send over\r\n");
                break;
            }
        }
    }

    // 4. 接收并验证数据 (需要外部硬件将TX与RX短接才能收到回环数据)
    uint32_t wait_timeout = 0;
    while (1) 
	{
        ret = usbh_serial_read(serial, &serial_rx_data[serial_rx_bytes], SERIAL_TEST_LEN - serial_rx_bytes);
        if (ret < 0) 
		{
            USB_LOG_RAW("serial read error, ret:%d\r\n", ret);
            return -1;
        } else 
		{
            serial_rx_bytes += ret;

            if (serial_rx_bytes == SERIAL_TEST_LEN) 
			{
                USB_LOG_RAW("receive over\r\n");
                for (uint32_t i = 0; i < SERIAL_TEST_LEN; i++) {
                    if (serial_rx_data[i] != 0xA5) {
                        USB_LOG_RAW("loopback data error at index %d", i);
                        return -1;
                    }
                }
                serial_test_success = true;
                break;
            }
        }
        wait_timeout++;

        if (wait_timeout > 100) { // 100 * 10ms = 1s
            USB_LOG_RAW("serial read timeout (Please check if TX and RX are shorted)\r\n");
            return -1;
        }
        Delay_ms(10);
    }

    if (serial_test_success) {
        USB_LOG_RAW("serial loopback test success\r\n");
    } else {
        USB_LOG_RAW("serial loopback test failed\r\n");
    }
    // 5. 关闭设备
    usbh_serial_close(serial);
    return (serial_test_success ? 0 : -1);
}

/* 当串口设备枚举成功后，底层会调用 run */
void usbh_serial_run(struct usbh_serial *serial)
{
	Delay_ms(10000);
    g_serial_dev = serial;
    g_usbh_serial_connected = 1;
    g_usbh_serial_tested = 0; // 每次新插入重置执行标志
}

/* 当串口设备拔出时，底层会调用 stop */
void usbh_serial_stop(struct usbh_serial *serial)
{
    if (g_serial_dev == serial) {
        g_serial_dev = NULL;
    }
    g_usbh_serial_connected = 0;
}

/* ================= 提供给 usb_task 的统一接口 ================= */

// 1. 初始化
void usbh_serial_init(uint8_t busid, uint32_t reg_base)
{
    g_usbh_serial_connected = 0;
    g_usbh_serial_tested = 0;
    g_serial_dev = NULL;
    usbh_initialize(busid, reg_base, NULL);
}

// 2. 反初始化
void usbh_serial_deinit(void)
{
    usbh_deinitialize(0);
    g_usbh_serial_connected = 0;
    g_usbh_serial_tested = 0;
    g_serial_dev = NULL;
}

// 3. 业务任务 (由 USB_Task 循环调用)
void usbh_serial_task(void)
{
    // 只有在串口连接成功，并且尚未执行过测试的情况下才去读写
    if (g_usbh_serial_connected && !g_usbh_serial_tested) 
    {
        usb_serial_test();
        g_usbh_serial_tested = 1;
    }
    Delay_ms(20);
}
