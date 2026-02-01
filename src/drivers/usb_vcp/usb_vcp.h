/*
 * usb_vcp.h
 *
 *  Created on: Jan 30, 2025
 *      Author: MrTransistor
 */

#pragma once

#include <string.h>

#include "drivers/usb/usb.h"

#define USBD_VID                0xf055
#define USBD_PID                0x0001
#define USBD_LANGID             0x409   // en-US

#define USB_CLASS_CDC           0x2
#define USB_CLASS_CDC_DATA      0xA
#define USB_SUBCLASS_ACM        0x2

// Descriptor types for CDC devices
#define DESCRIPTOR_CS_INTERFACE 0x24

// Functional descriptor subtypes
#define SUBTYPE_HEADER          0x0
#define SUBTYPE_CALL_MANAGEMENT 0x1
#define SUBTYPE_ACM             0x2
#define SUBTYPE_UNION           0x6

// Line coding response structure
typedef struct __attribute__ ((packed)) {
    uint32_t dwDTERate;
    uint8_t bCharFormat;
    uint8_t bParityType;
    uint8_t bDataBits;
} LineCoding_t;

// Control line state structure
typedef struct __attribute__ ((packed)) {
    uint8_t DTR :1;
    uint8_t RTS :1;
} ControlLineState_t;

// CDC Communications Class requests
#define CDC_SET_LINE_CODING         0x20
#define CDC_GET_LINE_CODING         0x21
#define CDC_SET_CONTROL_LINE_STATE  0x22
#define CDC_SEND_BREAK              0x23

#define CDC_DATA_ENDPOINT           2
#define CDC_DATA_EP_MAX_PACKET      64

#define VCP_BUF_LEN                 1024

/***************** Functions for sending and receiving data *****************/

/**
 * @brief  Get the number of bytes in RX buffer
 * @retval Number of bytes in RX buffer
 */
extern size_t VCP_available(void);

/**
 * @brief  Read last byte from RX buffer without removing it
 * @retval Last byte in RX buffer
 */
extern uint8_t VCP_peek(void);

/**
 * @brief  Read last byte from RX buffer
 * @retval Last byte in RX buffer
 */
extern uint8_t VCP_read(void);

/**
 * @brief  Read multiple bytes over VCP
 * @param  buf Pointer to the buffer to store data
 * @param  len Length of the data to be received
 * @retval None
 */
extern void VCP_readBytes(void*, size_t);

/**
 * @brief  Send a byte over VCP
 * @param  byte Byte to send
 * @retval None
 */
extern size_t VCP_write(uint8_t);

/**
 * @brief  Send multiple bytes over VCP
 * @param  data Pointer to the data
 * @param  len Length of the data to be sent
 * @retval None
 */
extern size_t VCP_writeBytes(const void*, size_t);

/**
 * @brief  Sent a Null-terminated string (NTS, C-string) over VCP
 * @param  str pointer to the null-terminated byte sequence
 * @retval None
 */
extern size_t VCP_sendNTS(char*);

/****************************************************************************/
