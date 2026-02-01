/*
 * usb_stdreq.h
 *
 *  Created on: Nov 9, 2023
 *      Author: MrTransistor
 */

#pragma once

#include <stdint.h>

#include "usb.h"

// Standard request types
#define STD_REQ_GET_STATUS		0x0
#define STD_REQ_CLEAR_FEATURE	0x1
#define STD_REQ_SET_FEATURE		0x2
#define STD_REQ_SET_ADDRESS		0x5
#define STD_REQ_GET_DESCRIPTOR	0x6
#define STD_REQ_SET_DESCRIPTOR	0x7
#define STD_REQ_GET_CONFIG		0x8
#define STD_REQ_SET_CONFIG		0x9
#define STD_REQ_GET_INTERFACE	0xa
#define STD_REQ_SET_INTERFACE	0xb
#define STD_REQ_SYNC_FRAME		0xc

#define STRING_DESC_LEN(n)      (stringDescriptors[n][0])

extern void HandleStandardRequest(uint8_t, SetupRequest_t*);


extern void ConfigFinishedHandler(uint8_t);

