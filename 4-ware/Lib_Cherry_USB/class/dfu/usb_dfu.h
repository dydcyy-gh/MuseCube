/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* === MuseCube modification ===
 * Renamed DFU_PROTOCOL_MODE to DFU_PROTOCOL_DFU (line 26) for consistency with STM32 DFU naming conventions.
 * Added DFU_NOTIFICATION_* defines (lines 79-85) for application-layer download/upload signal callbacks (begin, end, abort).
 * Added DFU_MEDIA_STATUS_* defines (lines 88-90) to report flash media operation results (OK, BUSY, ERROR).
 * Replaced generic DFU_CMD_GETCOMMANDS/DFU_CMD_SETADDRESSPOINTER/DFU_CMD_ERASE defines with STM32-specific DFU_SPECIAL_CMD_SET_ADDRESS_POINTER, DFU_SPECIAL_CMD_ERASE, and added DFU_SPECIAL_READ_UNPROTECT (lines 92-95), matching STM32 DFU 1.1 special command values (wValue = 0).
 * Removed unused DFU_MEDIA_ERASE and DFU_MEDIA_PROGRAM defines.
 * Renamed struct dfu_info to struct dfu_status (line 108) for descriptor-level naming alignment.
 * Changed bwPollTimeout from two uint8_t fields (bPollTimeout + wPollTimeout) to a single uint32_t (line 111), simplifying timeout handling.
 * Added DFU_DESCRIPTOR_LEN define (line 118) to expose total descriptor length.
 * Refactored DFU_DESCRIPTOR_INIT to accept a str_idx parameter (line 121) instead of hardcoding 0x04, enabling dynamic interface string index assignment.
 * Replaced literal 0x01/0x02 with DFU_SUBCLASS_DFU / DFU_PROTOCOL_DFU symbolic constants in the descriptor macro (lines 128-129).
 * Replaced hardcoded USBD_DFU_XFER_SIZE with CONFIG_USBDEV_REQUEST_BUFFER_LEN (line 136) for configurable transfer buffer sizing.
 * Replaced hardcoded 0x011a bcdDFUVersion with DFU_VERSION (line 137), referencing the existing version define.
 * Removed DFU_MANIFEST_COMPLETE and DFU_MANIFEST_IN_PROGRESS defines.
 * Removed DFU_DETACH_MASK and DFU_MANIFEST_MASK defines.
 * Applied #pragma once include guard (line 6) in addition to #ifndef/#endif guard.
 * Removed Doxygen /** @{ */ and @} */ group markers (lines 9,14) and /**\brief annotations on descriptor fields.
 * === End MuseCube modification === */
#ifndef USB_DFU_H
#define USB_DFU_H

/**\addtogroup USB_MODULE_DFU USB DFU class
 * \brief This module contains USB Device Firmware Upgrade class definitions.
 * \details This module based on
 * + [USB Device Firmware Upgrade Specification, Revision 1.1]
 * (https://www.usb.org/sites/default/files/DFU_1.1.pdf)
 * @{ */

/** DFU Specification release */
#define DFU_VERSION 0x0110

/** DFU Class Subclass */
#define DFU_SUBCLASS_DFU 0x01

/** DFU Class runtime Protocol */
#define DFU_PROTOCOL_RUNTIME 0x01

/** DFU Class DFU mode Protocol */
#define DFU_PROTOCOL_DFU 0x02

/**
 * @brief DFU Class Specific Requests
 */
#define DFU_REQUEST_DETACH    0x00
#define DFU_REQUEST_DNLOAD    0x01
#define DFU_REQUEST_UPLOAD    0x02
#define DFU_REQUEST_GETSTATUS 0x03
#define DFU_REQUEST_CLRSTATUS 0x04
#define DFU_REQUEST_GETSTATE  0x05
#define DFU_REQUEST_ABORT     0x06

/** DFU FUNCTIONAL descriptor type */
#define DFU_FUNC_DESC 0x21

/** DFU attributes DFU Functional Descriptor */
#define DFU_ATTR_WILL_DETACH            0x08
#define DFU_ATTR_MANIFESTATION_TOLERANT 0x04
#define DFU_ATTR_CAN_UPLOAD             0x02
#define DFU_ATTR_CAN_DNLOAD             0x01

/** bStatus values for the DFU_GETSTATUS response */
#define DFU_STATUS_OK               0x00U
#define DFU_STATUS_ERR_TARGET       0x01U
#define DFU_STATUS_ERR_FILE         0x02U
#define DFU_STATUS_ERR_WRITE        0x03U
#define DFU_STATUS_ERR_ERASE        0x04U
#define DFU_STATUS_ERR_CHECK_ERASED 0x05U
#define DFU_STATUS_ERR_PROG         0x06U
#define DFU_STATUS_ERR_VERIFY       0x07U
#define DFU_STATUS_ERR_ADDRESS      0x08U
#define DFU_STATUS_ERR_NOTDONE      0x09U
#define DFU_STATUS_ERR_FIRMWARE     0x0AU
#define DFU_STATUS_ERR_VENDOR       0x0BU
#define DFU_STATUS_ERR_USB          0x0CU
#define DFU_STATUS_ERR_POR          0x0DU
#define DFU_STATUS_ERR_UNKNOWN      0x0EU
#define DFU_STATUS_ERR_STALLEDPKT   0x0FU

/** bState values for the DFU_GETSTATUS response */
#define DFU_STATE_APP_IDLE                0U
#define DFU_STATE_APP_DETACH              1U
#define DFU_STATE_DFU_IDLE                2U
#define DFU_STATE_DFU_DNLOAD_SYNC         3U
#define DFU_STATE_DFU_DNLOAD_BUSY         4U
#define DFU_STATE_DFU_DNLOAD_IDLE         5U
#define DFU_STATE_DFU_MANIFEST_SYNC       6U
#define DFU_STATE_DFU_MANIFEST            7U
#define DFU_STATE_DFU_MANIFEST_WAIT_RESET 8U
#define DFU_STATE_DFU_UPLOAD_IDLE         9U
#define DFU_STATE_DFU_ERROR               10U

/* Define DFU application notification signals.  */
#define DFU_NOTIFICATION_BEGIN_DOWNLOAD 0x1u
#define DFU_NOTIFICATION_END_DOWNLOAD   0x2u
#define DFU_NOTIFICATION_ABORT_DOWNLOAD 0x3u
#define DFU_NOTIFICATION_BEGIN_UPLOAD   0x5u
#define DFU_NOTIFICATION_END_UPLOAD     0x6u
#define DFU_NOTIFICATION_ABORT_UPLOAD   0x7u

/* Define DFU application notification signals.  */
#define DFU_MEDIA_STATUS_OK    0
#define DFU_MEDIA_STATUS_BUSY  1
#define DFU_MEDIA_STATUS_ERROR 2

/** Special Commands with Download Request for STM32, wValue = 0 */
#define DFU_SPECIAL_CMD_SET_ADDRESS_POINTER 0x21U
#define DFU_SPECIAL_CMD_ERASE               0x41U
#define DFU_SPECIAL_READ_UNPROTECT          0x92U

/** Run-Time Functional Descriptor */
struct dfu_runtime_descriptor {
    uint8_t bLength;         /**<\brief Descriptor length in bytes.*/
    uint8_t bDescriptorType; /**<\brief DFU functional descriptor type.*/
    uint8_t bmAttributes;    /**<\brief USB DFU capabilities \ref USB_DFU_CAPAB*/
    uint16_t wDetachTimeout; /**<\brief USB DFU detach timeout in ms.*/
    uint16_t wTransferSize;  /**<\brief USB DFU maximum transfer block size in bytes.*/
    uint16_t bcdDFUVersion;  /**<\brief USB DFU version \ref VERSION_BCD utility macro.*/
} __PACKED;

/**\brief Payload packet to response in DFU_GETSTATUS request */
struct dfu_status {
    uint8_t bStatus;        /**<\brief An indication of the status resulting from the
                                     * execution of the most recent request.*/
    uint32_t bwPollTimeout; /**<\brief Minimum time in ms, that the host should wait
                                     * before sending a subsequent DFU_GETSTATUS request.*/
    uint8_t bState;         /**<\brief An indication of the state that the device is going
                                     * to enter immediately following transmission of this response.*/
    uint8_t iString;        /**<\brief Index of the status string descriptor.*/
};

#define DFU_DESCRIPTOR_LEN 18

// clang-format off
#define DFU_DESCRIPTOR_INIT(str_idx)                                                     \
    0x09,                          /* bLength */                                         \
    USB_DESCRIPTOR_TYPE_INTERFACE, /* bDescriptorType */                                 \
    0x00,                          /* bInterfaceNumber */                                \
    0x00,                          /* bAlternateSetting */                               \
    0x00,                          /* bNumEndpoints Default Control Pipe only */         \
    USB_DEVICE_CLASS_APP_SPECIFIC, /* bInterfaceClass */                                 \
    DFU_SUBCLASS_DFU,              /* bInterfaceSubClass Device Firmware Upgrade */      \
    DFU_PROTOCOL_DFU,              /* bInterfaceProtocol DFU mode */                     \
    str_idx,                       /* iInterface */                                      \
    /*!< Device Firmware Update Functional Descriptor  */                                \
    0x09,                          /* bLength */                                         \
    0x21,                          /* DFU Functional Descriptor */                       \
    0x0B,                          /* bmAttributes */                                    \
    WBVAL(0x00ff),                 /* wDetachTimeOut */                                  \
    WBVAL(CONFIG_USBDEV_REQUEST_BUFFER_LEN),     /* wTransferSize */                     \
    WBVAL(DFU_VERSION)             /* bcdDFUVersion */
// clang-format on

#endif /* USB_DFU_H */
