/*
 * usb_stdreq.c
 *
 *  Created on: Nov 9, 2023
 *      Author: MrTransistor
 */
#include "usb_stdreq.h"

// USB connection state (default/not configured, address stage, configured)
extern volatile USBConnState_e connState;
// The address to set after the next transaction. 0xFF means no change
extern volatile uint8_t addressToSet;

//******************* Available for user to be implemented **********************/

__attribute__ ((weak)) void HandleOtherDescriptors(uint8_t epNum, SetupRequest_t *req) {
    // If not implemented but received a request, trigger a breakpoint
    asm("bkpt");
}

__attribute__ ((weak)) void ConfigFinishedHandler(uint8_t configNum) {

}

/********************************************************************************/

USBConnState_e USB_getConnState(void) {
    return connState;
}

static inline void HandleSetAddress(uint8_t epNum, SetupRequest_t *req) {
    if (connState == CONN_STATE_CONFIGURED) {
        // Not supported when already configured
        StallEP(epNum | 0x80);
        return;
    }
    addressToSet = req->wValue;
    StateStageIn(epNum, 0, 0);
    // Everything else is going to happen in the CTR handler
}

static inline void HandleGetDescriptor(uint8_t epNum, SetupRequest_t *req) {
    switch (req->wValue >> 8) {
        case DESCRIPTOR_DEVICE:
            InitiateTransfer(epNum, (uint8_t*) deviceDescriptor, (req->wLength < DEVICE_DESC_LEN ? req->wLength : DEVICE_DESC_LEN), &StateStageOut);
            break;
        case DESCRIPTOR_CONFIG:
            InitiateTransfer(epNum, (uint8_t*) configDescriptor, (req->wLength < CONFIG_DESC_LEN ? req->wLength : CONFIG_DESC_LEN), &StateStageOut);
            break;
        case DESCRIPTOR_STRING:
            uint8_t descrNum = req->wValue & 0xFF;
            if (descrNum >= STRING_DESC_NUM) {
                StallEP(epNum | 0x80);
                break;
            }
            InitiateTransfer(epNum, (uint8_t*) stringDescriptors[descrNum],
                    (req->wLength < STRING_DESC_LEN(descrNum)) ? req->wLength : STRING_DESC_LEN(descrNum), &StateStageOut);
            break;
        case DESCRIPTOR_DEVQUALIFIER:
            StallEP(epNum | 0x80);
            break;
        default:
            HandleOtherDescriptors(epNum, req);
    }
}

// TODO: maybe add ability to have multiple configurations
// This would also require having a way to notify user about configuration change
// Maybe it is not required and the user must poll configNum value and switch configs in case it changes
// Also USB spec requires DTOG flags of all interface endpoints must be reset to DATA0 after configuration switch
static inline void HandleGetConfiguration(uint8_t epNum, SetupRequest_t *req) {
    static uint8_t confNum;
    switch (connState) {
        case CONN_STATE_DEFAULT:
            // Behavior not specified in default state
            StallEP(epNum | 0x80);
            break;
        case CONN_STATE_ADDRESS:
            confNum = 0;
            InitiateTransfer(epNum, &confNum, 1, &StateStageOut);
            break;
        case CONN_STATE_CONFIGURED:
            confNum = 1;
            InitiateTransfer(0, &confNum, 1, &StateStageOut);
            break;
    }
}

static inline void HandleSetConfiguration(uint8_t epNum, SetupRequest_t *req) {
    if (connState == CONN_STATE_DEFAULT) {
        // Not supported when address not set
        StallEP(epNum | 0x80);
        return;
    }
    switch (req->wValue) {
        case 0:
            connState = CONN_STATE_ADDRESS;
            StateStageIn(epNum, 0, 0);
            break;
        case 1:
            connState = CONN_STATE_CONFIGURED;
            StateStageIn(epNum, 0, 0);
            ConfigFinishedHandler(1);
            break;
        default:
            StallEP(epNum | 0x80);
    }
}

inline void HandleStandardRequest(uint8_t epNum, SetupRequest_t *req) {
    // Note: Requests that are not core-specific are also passed to the user
    switch (req->bRequest) {
        case STD_REQ_SET_ADDRESS:
            HandleSetAddress(epNum, req);
            break;
        case STD_REQ_GET_DESCRIPTOR:
            HandleGetDescriptor(epNum, req);
            break;
        case STD_REQ_GET_CONFIG:
            HandleGetConfiguration(epNum, req);
            break;
        case STD_REQ_SET_CONFIG:
            HandleSetConfiguration(epNum, req);
            break;
        default:
            HandleOtherRequests(epNum, req);
            break;
    }
}
