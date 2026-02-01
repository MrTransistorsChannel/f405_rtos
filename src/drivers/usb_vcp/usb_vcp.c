/*
 * usb_vcp.c
 *
 *  Created on: Jan 6, 2025
 *      Author: MrTransistor
 */

#include "usb_vcp.h"

// Device descriptor
const uint8_t deviceDescriptor[] = {
        18,                     // bLength
        DESCRIPTOR_DEVICE,      // bDescriptorType

        0x00,                   // bcdUSB   Binary coded decimal for USB revision
        0x02,

        USB_CLASS_CDC,          // bDeviceClass     Communications and CDC Control
        0x00,                   // bDeviceSubClass
        0x00,                   // bDeviceProtocol

        64,                     // bMaxPacketSize   Max packet size for ep0

        USBD_VID & 0xff,        // idVendor
        USBD_VID >> 8,          // idVendor
        USBD_PID & 0xff,        // idProduct
        USBD_PID >> 8,          // idProduct

        0x01,                   // bcdDevice        Binary coded decimal for device release number
        0x00,

        0x01,                   // iManufacturer    Index of manufacturer string
        0x02,                   // iProduct         Index of product string
        0x03,                   // iSerialNumber    Index of serial number string

        0x01                    // bNumConfigurations   Number of possible configurations
        };

// Configuration, interface and endpoint descriptors
const uint8_t configDescriptor[] = {
        // Configuration 1 descriptor
        0x09,// bLength
        DESCRIPTOR_CONFIG,      // bDescriptorType
        0x43,                   // wTotalLength     67 bytes
        0x00,                   // wTotalLength
        0x02,                   // bNumInterfaces
        0x01,                   // bConfigurationValue  Can`t be 0 because config 0 means go back to address state
        0x00,                   // iConfiguration   No string descriptor associated
        (1 << 7) | (1 << 6),    // bmAttributes     b7 - Reserved, b6 - Self-powered
        0xFA,                   // bMaxPower        500mA

        // Interface 0 descriptor CDC Communication interface
        0x09,// bLength
        DESCRIPTOR_INTERFACE,   // bDescriptorType
        0x00,                   // bInterfaceNumber
        0x00,                   // bAlternateSetting
        0x01,                   // bNumEndpoints    IN and OUT endpoints are counted separately
        USB_CLASS_CDC,          // bInterfaceClass  Communications and CDC Control class
        USB_SUBCLASS_ACM,       // bInterfaceSubclass
        0x00,                   // bInterfaceProtocol
        0x00,                   // iInterface       No string descriptor associated

        // Header Functional descriptor
        0x05,// bLength
        DESCRIPTOR_CS_INTERFACE,                   // bDescriptorType
        SUBTYPE_HEADER,         // bDescriptorSubtype
        0x10,                   // bcdCDC
        0x01,

        // ACM Functional descriptor
        0x04,// bLength
        DESCRIPTOR_CS_INTERFACE,                   // bDescriptortype
        SUBTYPE_ACM,            // bDescriptorsubtype
        0x02,                   // bmCapabilities   Supports subset of ACM commands

        // Union Functional descriptor
        0x05,// bLength
        DESCRIPTOR_CS_INTERFACE,                   // bDescriptortype
        SUBTYPE_UNION,          // bDescriptorsubtype
        0x00,                   // bControlInterface
        0x01,                   // bSubordinateInterface0

        // Call Management Functional descriptor
        0x05,// bLength
        DESCRIPTOR_CS_INTERFACE,                   // bDescriptortype
        SUBTYPE_CALL_MANAGEMENT,                   // bDescriptorsubtype
        0x03,                   // bmCapabilities   DIY (?)
        0x01,                   // bDataInterface

        // Notification endpoint descriptor
        0x07,// bLength
        DESCRIPTOR_ENDPOINT,    // bDescriptorType
        0x81,                   // bEndpointAddress Endpoint 1, Direction IN
        0x03,                   // bmAttributes     Interrupt
        0x40,                   // wMaxPacketSize   64 bytes
        0x00,                   // wMaxPacketSize
        0xFF,                   // bInterval

        /* CDC Data interface */
        0x09,                   // bLength
        DESCRIPTOR_INTERFACE,   // bDescriptorType
        0x01,                   // bInterfaceNumber
        0x00,                   // bAlternateSetting
        0x02,                   // bNumEndpoints
        USB_CLASS_CDC_DATA,     // bInterfaceClass
        0x00,                   // bInterfaceSubClass
        0x00,                   // bInterfaceProtocol
        0x00,                   // iInterface

        // Endpoint 2 OUT descriptor
        0x07,// bLength
        DESCRIPTOR_ENDPOINT,    // bDescriptorType
        0x02,                   // bEndpointAddress Endpoint 2, Direction OUT
        0x02,                   // bmAttributes     Bulk
        0x40,                   // wMaxPacketSize   64 bytes
        0x00,                   // wMaxPacketSize
        0x01,                   // bInterval        2ms

        // Endpoint 2 IN descriptor
        0x07,// bLength
        DESCRIPTOR_ENDPOINT,    // bDescriptorType
        0x82,                   // bEndpointAddress Endpoint 2, Direction IN
        0x02,                   // bmAttributes     Bulk
        0x40,                   // wMaxPacketSize   64 bytes
        0x00,                   // wMaxPacketSize
        0x01,                   // bInterval        2ms
        };

const uint8_t langIDDescriptor[] = {
        0x04,                   // bLength
        DESCRIPTOR_STRING,      // bDescriptorType
        USBD_LANGID & 0xFF,     // wLANGID[0]
        USBD_LANGID >> 8        // wLANGID[0]
};

const uint16_t manufacturerString[] = {
        26 | (DESCRIPTOR_STRING << 8),                  // bLength|bDescriptorType
        'M', 'r', 'T', 'r', 'a', 'n', 's', 'i', 's', 't', 'o', 'r' // bString
        };

const uint16_t productString[] = {
        34 | (DESCRIPTOR_STRING << 8),                  // bLength|bDescriptorType
        'V', 'i', 'r', 't', 'u', 'a', 'l', ' ', 'C', 'O', 'M', ' ', 'P', 'o', 'r', 't' // bString
        };

const uint16_t versionString[] = {
        14 | (DESCRIPTOR_STRING << 8),                  // bLength|bDescriptorType
        'v', '1', '.', '0', '.', '0'                         // bString
        };

const uint8_t *const stringDescriptors[] = {
        langIDDescriptor,
        (const uint8_t*) manufacturerString,
        (const uint8_t*) productString,
        (const uint8_t*) versionString
};

const uint8_t STRING_DESC_NUM = sizeof(stringDescriptors) / sizeof(stringDescriptors[0]);

LineCoding_t lineCoding = {
        115200,     // 115200 baud
        0,          // 1 stop bit
        0,          // No parity
        8           // 8 data bits
        };

ControlLineState_t controlLineState;

uint8_t txBuf[VCP_BUF_LEN];
uint8_t txTmpBuf[CDC_DATA_EP_MAX_PACKET];
uint16_t txBufHead;
uint16_t txBufTail;

uint8_t rxBuf[VCP_BUF_LEN];
uint8_t rxTmpBuf[CDC_DATA_EP_MAX_PACKET];
uint16_t rxBufHead;
uint16_t rxBufTail;

// Private functions

void _handleDataTransmission(uint8_t epNum) {
    // If the buffer is already empty
    if (txBufHead == txBufTail)
        return;

    // If have data to send, unload up to CDC_EP_MAX_PACKET bytes at a time
    uint8_t *txBufPtr = txTmpBuf;

    for (size_t len = 0; len < CDC_DATA_EP_MAX_PACKET; len++) {
        uint8_t byte = txBuf[txBufTail++];
        if (txBufTail == VCP_BUF_LEN)
            txBufTail = 0;

        *txBufPtr++ = byte;

        // If the buffer is empty
        if (txBufHead == txBufTail) {
            break;
        }
    }

    InitiateTransfer(epNum, txTmpBuf, (size_t) (txBufPtr - txTmpBuf), &_handleDataTransmission);
}

void _handleDataReception(uint8_t epNum, void *buf, size_t len) {
    while (len--) {
        uint16_t next = rxBufHead + 1;
        if (next == VCP_BUF_LEN)
            next = 0;

        if (next == rxBufTail)
            break;

        rxBuf[rxBufHead] = *((uint8_t*) buf++);
        rxBufHead = next;
    }

    InitiateReception(epNum, rxTmpBuf, CDC_DATA_EP_MAX_PACKET, &_handleDataReception);
}

// Public functions
size_t VCP_available(void) {
    int16_t available = rxBufHead - rxBufTail;
    if (available < 0)
        available += VCP_BUF_LEN;
    return available;
}

uint8_t VCP_peek(void) {
    return (rxBufHead == rxBufTail) ? -1 : rxBuf[rxBufTail];
}

uint8_t VCP_read(void) {
    if (rxBufHead == rxBufTail)
        return -1;

    uint8_t byte = rxBuf[rxBufTail++];
    if (rxBufTail == VCP_BUF_LEN)
        rxBufTail = 0;
    return byte;
}

void VCP_readBytes(void *buf, size_t len) {
    while (len--)
        *((uint8_t*) buf++) = VCP_read();
}

size_t VCP_write(uint8_t byte) {
    if (VCP_writeBytes(&byte, 1) == 0)
        return 0;
    else
        return 1;
}

/*size_t VCP_writeBytes(const void *data, size_t len) {
 uint16_t bytesLeft = len;
 // If no transmission in progress and are connected to USB, send one packet immediately
 if (!txBusy(CDC_DATA_ENDPOINT) && USB_getConnState() == CONN_STATE_CONFIGURED) {
 uint16_t thisPacketLen = (len < CDC_DATA_EP_MAX_PACKET) ? len : CDC_DATA_EP_MAX_PACKET;

 memcpy(txTmpBuf, data, thisPacketLen);
 InitiateTransfer(CDC_DATA_ENDPOINT, txTmpBuf, thisPacketLen, &_handleDataTransmission);

 // If already sent all of the data
 if (len == thisPacketLen)
 return len;

 // If some data remains, advance the data pointer forward
 data += CDC_DATA_EP_MAX_PACKET;
 bytesLeft -= CDC_DATA_EP_MAX_PACKET;
 }

 // Try to load data into TX buffer
 while (bytesLeft--) {
 uint16_t next = txBufHead + 1;
 if (next == VCP_BUF_LEN)
 next = 0;

 // If the buffer is full, drop any pending data to avoid blocking
 if (next == txBufTail)
 return len - bytesLeft;

 txBuf[txBufHead] = *((uint8_t*) data++);
 txBufHead = next;
 }
 // If all of the data were sent/loaded
 return len;
 }*/

size_t VCP_writeBytes(const void *data, size_t len) {
    size_t bytesLeft = len;
    while (bytesLeft--) {
        uint16_t next = txBufHead + 1;
        if (next == VCP_BUF_LEN)
            next = 0;

        // If the buffer is full, drop any pending data to avoid blocking
        if (next == txBufTail)
            break;

        txBuf[txBufHead] = *((uint8_t*) data++);
        txBufHead = next;
    }

    if (!txBusy(CDC_DATA_ENDPOINT) && USB_getConnState() == CONN_STATE_CONFIGURED)
        _handleDataTransmission(CDC_DATA_ENDPOINT);
    // If all of the data were sent/loaded
    return len - bytesLeft;
}

size_t VCP_sendNTS(char *str) {
    char *_str = str;
    size_t len = 0;
    while (*_str++ != '\0')
        len++;

    return VCP_writeBytes(str, len);
}

void ConfigFinishedHandler(uint8_t configNum) {
    // EP1 config
    MODIFY_REG(USB_OTG_InEP(1)->DIEPCTL, USB_OTG_DIEPCTL_TXFNUM, (1 << USB_OTG_DIEPCTL_TXFNUM_Pos));
    // Unmask EP1 interrupts
    USB_OTG_DEV->DAINTMSK |= ((1 << 1) << USB_OTG_DAINTMSK_IEPM_Pos) | ((1 << 1) << USB_OTG_DAINTMSK_OEPM_Pos);
    // Set max packet size to 64 bytes for EP2
    MODIFY_REG(USB_OTG_InEP(1)->DIEPCTL, USB_OTG_DIEPCTL_MPSIZ, 64);
    EP_max_packet[1 | 0x8] = 64;
    MODIFY_REG(USB_OTG_OutEP(1)->DOEPCTL, USB_OTG_DOEPCTL_MPSIZ, 64);
    EP_max_packet[1] = 64;

    USB_OTG_InEP(1)->DIEPCTL |= USB_OTG_DIEPCTL_USBAEP | USB_OTG_DIEPCTL_SNAK | (0b11 << USB_OTG_DIEPCTL_EPTYP_Pos);
    USB_OTG_OutEP(1)->DOEPCTL |= USB_OTG_DOEPCTL_USBAEP | USB_OTG_DOEPCTL_SNAK | (0b11 << USB_OTG_DOEPCTL_EPTYP_Pos);

    // EP2 config
    MODIFY_REG(USB_OTG_InEP(2)->DIEPCTL, USB_OTG_DIEPCTL_TXFNUM, (2 << USB_OTG_DIEPCTL_TXFNUM_Pos));
    // Unmask EP2 interrupts
    USB_OTG_DEV->DAINTMSK |= ((1 << 2) << USB_OTG_DAINTMSK_IEPM_Pos) | ((1 << 2) << USB_OTG_DAINTMSK_OEPM_Pos);
    // Set max packet size to 64 bytes for EP2
    MODIFY_REG(USB_OTG_InEP(2)->DIEPCTL, USB_OTG_DIEPCTL_MPSIZ, 64);
    EP_max_packet[2 | 0x8] = 64;
    MODIFY_REG(USB_OTG_OutEP(2)->DOEPCTL, USB_OTG_DOEPCTL_MPSIZ, 64);
    EP_max_packet[2] = 64;

    USB_OTG_InEP(2)->DIEPCTL |= USB_OTG_DIEPCTL_USBAEP | USB_OTG_DIEPCTL_SNAK | (0b10 << USB_OTG_DIEPCTL_EPTYP_Pos);
    USB_OTG_OutEP(2)->DOEPCTL |= USB_OTG_DOEPCTL_USBAEP | USB_OTG_DOEPCTL_SNAK | (0b10 << USB_OTG_DOEPCTL_EPTYP_Pos);

    InitiateReception(CDC_DATA_ENDPOINT, rxTmpBuf, CDC_DATA_EP_MAX_PACKET, &_handleDataReception);
}

void HandleOtherRequests(uint8_t epNum, SetupRequest_t *req) {
    switch (req->bRequest) {
        case CDC_SET_LINE_CODING:
            InitiateReception(epNum, (uint8_t*) &lineCoding, sizeof(lineCoding), &StateStageIn);
            break;
        case CDC_GET_LINE_CODING:
            InitiateTransfer(epNum, (uint8_t*) &lineCoding, sizeof(lineCoding), &StateStageOut);
            break;
        case CDC_SET_CONTROL_LINE_STATE:
            *((uint8_t*) &controlLineState) = (uint8_t) req->wValue;
            StateStageIn(epNum, 0, 0);
            break;
        default:
            asm("bkpt");
    }
}

