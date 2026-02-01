/*
 * usb.h
 *
 *  Created on: Aug 23, 2024
 *      Author: MrTransistor
 */

#pragma once

#include <stm32f4xx.h>
#include <stddef.h>

/*************** MCU-specific things ********************/
#define USB_IRQ_Handler     OTG_FS_IRQHandler

#define USB_OTG_BASE        USB_OTG_FS_PERIPH_BASE
#define USB_OTG             ((USB_OTG_GlobalTypeDef*)USB_OTG_BASE)
#define USB_OTG_DEV         ((USB_OTG_DeviceTypeDef*)(USB_OTG_BASE + USB_OTG_DEVICE_BASE))
#define USB_OTG_PCGCCTL     *(__IO uint32_t *)(USB_OTG_BASE + USB_OTG_PCGCCTL_BASE)

#define USB_OTG_HW_EP_NUM   4

#define USB_OTG_InEP(i)     ((USB_OTG_INEndpointTypeDef*)(USB_OTG_BASE + USB_OTG_IN_ENDPOINT_BASE \
                                    + ((i)*0x20)))
#define USB_OTG_OutEP(i)    ((USB_OTG_OUTEndpointTypeDef*)(USB_OTG_BASE + USB_OTG_OUT_ENDPOINT_BASE \
                                    + ((i)*0x20)))

#define USB_OTG_FIFO(i)     *(__IO uint32_t *)(USB_OTG_BASE + USB_OTG_FIFO_BASE + ((i) * USB_OTG_FIFO_SIZE))

#define USB_OTG_AllInEPInterrupts       (USB_OTG_DEV->DAINT & USB_OTG_DEV->DAINTMSK & 0xFFFF)
#define USB_OTG_AllOutEPInterrupts      ((USB_OTG_DEV->DAINT & USB_OTG_DEV->DAINTMSK) >> 16)
#define USB_OTG_InEPInterrupts(i)       ((USB_OTG_InEP(i)->DIEPINT) & ((USB_OTG_DEV->DIEPMSK) \
                                                | ((((USB_OTG_DEV->DIEPEMPMSK) >> (i)) & 0x1) << 7)))
#define USB_OTG_OutEPInterrupts(i)      ((USB_OTG_OutEP(i)->DOEPINT) & (USB_OTG_DEV->DOEPMSK))

#define USB_OTG_GRXSTSP_PKTSTS_DATA     (0b10 << USB_OTG_GRXSTSP_PKTSTS_Pos)
#define USB_OTG_GRXSTSP_PKTSTS_SETUP    (0b110 << USB_OTG_GRXSTSP_PKTSTS_Pos)

typedef enum {
    USB_MODE_DEVICE,
    USB_MODE_HOST,
    USB_MODE_DRD
} USB_OTG_Mode_t;

/******************** Hardware layer ********************/
// USB connection states
typedef enum {
    CONN_STATE_DEFAULT = 0,
    CONN_STATE_ADDRESS,
    CONN_STATE_CONFIGURED
} USBConnState_e;

/******************** Protocol layer ********************/
// USB control request directions
#define REQUEST_DIR_TO_DEVICE   0
#define REQUEST_DIR_TO_HOST     1

// USB control request types
#define REQUEST_TYPE_STANDARD   0
#define REQUEST_TYPE_CLASS      1
#define REQUEST_TYPE_VENDOR     2

// USB control request recipients
#define REQUEST_RECIP_DEVICE    0
#define REQUEST_RECIP_INTERFACE 1
#define REQUEST_RECIP_ENDPOINT  2
#define REQUEST_RECIP_OTHER     3

// USB standard requests
#define REQUEST_GET_STATUS      0
#define REQUEST_CLEAR_FEATURE   1
#define REQUEST_SET_FEATURE     3
#define REQUEST_SET_ADDRESS     5
#define REQUEST_GET_DESCRIPTOR  6
#define REQUEST_SET_DESCRIPTOR  7
#define REQUEST_GET_CONFIG      8
#define REQUEST_SET_CONFIG      9
#define REQUEST_GET_INTERFACE   10
#define REQUEST_SET_INTERFACE   11
#define REQUEST_SYNCH_FRAME     12

// USB descriptor types
#define DESCRIPTOR_DEVICE       1
#define DESCRIPTOR_CONFIG       2
#define DESCRIPTOR_STRING       3
#define DESCRIPTOR_INTERFACE    4
#define DESCRIPTOR_ENDPOINT     5
#define DESCRIPTOR_DEVQUALIFIER 6

extern const uint8_t deviceDescriptor[];
extern const uint8_t configDescriptor[];
extern const uint8_t *const stringDescriptors[];
extern const uint8_t stringDescriptorLengths[];

#define DEVICE_DESC_LEN         18
#define CONFIG_DESC_LEN         (configDescriptor[2])
extern const uint8_t STRING_DESC_NUM;

/******************** Typedefs *********************/
typedef struct __attribute__ ((packed)) {
    // bmRequestType
    uint8_t recipient :5;
    uint8_t type :2;
    uint8_t direction :1;

    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} SetupRequest_t;

/******************** Variables ********************/
extern uint16_t EP_max_packet[16]; // TODO: this should be removed and the ep initialisation must be done in a function

/******************** Functions ********************/

extern USBConnState_e USB_getConnState(void);

void InitiateTransfer(uint8_t, const void*, size_t, const void (*completeHandler)(uint8_t));
void InitiateReception(uint8_t, void*, size_t, const void (*completeHandler)(uint8_t, void*, size_t));

void StateStageIn(uint8_t, void*, size_t);
void StateStageOut(uint8_t);

uint8_t txBusy(uint8_t);
uint8_t rxBusy(uint8_t);

void StallEP(uint8_t);

// USB hardware initialisation
void USB_init(void);

/*********** Weak handlers available for user to be implemented ***********/
void USB_ResetHandler(void);
void USB_StartOfFrameHandler(void);
void HandleOtherRequests(uint8_t, SetupRequest_t *req);
void HandleIncomingOutputPacket(void);
/**************************************************************************/
