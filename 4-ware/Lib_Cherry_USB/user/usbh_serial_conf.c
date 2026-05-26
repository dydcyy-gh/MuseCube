#include "usbh_serial_conf.h"
#include "usbh_core.h"
#include "usbh_serial.h"
#include "systick_conf.h" 
#include <string.h>

static volatile uint8_t g_usbh_serial_connected = 0;
static struct usbh_serial *g_serial_dev = NULL;
static bool g_serial_is_opened = false;

/* 当串口设备枚举成功后，底层会调用 run */
void usbh_serial_run(struct usbh_serial *serial)
{
    g_serial_dev = serial;
    g_usbh_serial_connected = 1;
    // 这里不要写延时，尽早退出让USB核心任务继续
    USB_LOG_RAW("USB Serial Device Plugged In!\r\n");
}

/* 当串口设备拔出时，底层会调用 stop */
void usbh_serial_stop(struct usbh_serial *serial)
{
    if (g_serial_dev == serial) {
        g_serial_dev = NULL;
    }
    g_usbh_serial_connected = 0;
    g_serial_is_opened = false;
    USB_LOG_RAW("USB Serial Device Unplugged!\r\n");
}

/* ================= 供应用层调用的接口实现 ================= */

bool app_usb_serial_is_connected(void)
{
    return (g_usbh_serial_connected && g_serial_dev != NULL);
}

int app_usb_serial_open(void)
{
    if (!app_usb_serial_is_connected()) return -1;
    if (g_serial_is_opened) return 0;

    // 尝试打开 CDC(ACM) 或 厂商自定义(如CH340/CP2102) 设备
    // 使用非阻塞模式，方便UI随时读取
    struct usbh_serial *serial = usbh_serial_open("/dev/ttyACM0", USBH_SERIAL_O_RDWR | USBH_SERIAL_O_NONBLOCK);
    if (serial == NULL) {
        serial = usbh_serial_open("/dev/ttyUSB0", USBH_SERIAL_O_RDWR | USBH_SERIAL_O_NONBLOCK);
    }
    
    if (serial != NULL) {
        g_serial_dev = serial; // 确保指向打开的实例
        g_serial_is_opened = true;
        
        // 默认配置 115200 8N1
        app_usb_serial_config(115200, 8, 0, 0);
        return 0;
    }
    return -1;
}

void app_usb_serial_close(void)
{
    if (g_serial_is_opened && g_serial_dev) {
        usbh_serial_close(g_serial_dev);
        g_serial_is_opened = false;
    }
}

int app_usb_serial_config(uint32_t baudrate, uint8_t data_bits, uint8_t parity, uint8_t stop_bits)
{
    if (!g_serial_is_opened || !g_serial_dev) return -1;

    struct usbh_serial_termios termios;
    memset(&termios, 0, sizeof(termios));
    termios.baudrate = baudrate;
    termios.databits = data_bits;
    termios.parity = parity;
    termios.stopbits = stop_bits;
    termios.rtscts = false;
    termios.rx_timeout = 0;

    return usbh_serial_control(g_serial_dev, USBH_SERIAL_CMD_SET_ATTR, &termios);
}

int app_usb_serial_send(const uint8_t *data, uint32_t len)
{
    if (!g_serial_is_opened || !g_serial_dev) return -1;
    return usbh_serial_write(g_serial_dev, data, len);
}

int app_usb_serial_recv(uint8_t *buffer, uint32_t max_len)
{
    if (!g_serial_is_opened || !g_serial_dev) return 0;
    
    // 由于打开时使用了 USBH_SERIAL_O_NONBLOCK，这里读不到数据会立即返回0
    int ret = usbh_serial_read(g_serial_dev, buffer, max_len);
    return (ret > 0) ? ret : 0; 
}


/* ================= 提供给 usb_task 的统一接口 ================= */

void usbh_serial_init(uint8_t busid, uint32_t reg_base)
{
    g_usbh_serial_connected = 0;
    g_serial_is_opened = false;
    g_serial_dev = NULL;
    usbh_initialize(busid, reg_base, NULL);
}

void usbh_serial_deinit(void)
{
    app_usb_serial_close();
    usbh_deinitialize(0);
    g_usbh_serial_connected = 0;
    g_serial_is_opened = false;
    g_serial_dev = NULL;
}

void usbh_serial_task(void)
{
    // 如果想要系统自动打开串口，可以在这里处理
    // 但通常对于串口助手来说，打开/关闭动作由用户在UI上点击按钮触发更好
    // 如果是由UI触发，这个 task 里其实什么都不用做，或者做一些状态维护即可
    Delay_ms(20);
}
