/*
 * usb.c
 *
 *  Created on: Aug 23, 2024
 *      Author: MrTransistor
 */

#include "usb.h"
#include "usb_stdreq.h"

// USB connection state (default/not configured, address stage, configured)
volatile USBConnState_e connState = CONN_STATE_DEFAULT;
// The address to set after the next transaction. 0xFF means no change
volatile uint8_t addressToSet = 0xFF;

// Used for allocated PMA bytes calculation
uint16_t bdTableUsedBytes;

// Pointers to buffers of data being sent
const uint8_t *TxData[8];
// Number of bytes left to send for each EP
volatile uint16_t TxDataLastSent[8];
volatile uint16_t TxDataLeft[8];

// This is needed for EP0 long transmission handling
volatile uint16_t EP0_txPacketsLeft;
volatile uint16_t EP0_txDataLeft;
// Handler function pointers for transmission completion
void (*txCompleteHandler[8])(uint8_t);

// Pointers to buffers of data being received
uint8_t *RxData[8];
// Number of bytes left to receive (maximum) for each EP
volatile uint16_t RxDataReceived[8];
volatile uint16_t RxDataLeft[8];
// Handler function pointers for reception completion
void (*rxCompleteHandler[8])(uint8_t, void*, size_t);

uint16_t EP_max_packet[16];

//******************* Available for user to be implemented **********************/

__attribute__ ((weak)) void USB_ResetHandler(void) {
    // Nothing here
}

__attribute__ ((weak)) void USB_StartOfFrameHandler(void) {
    // Nothing here
}

__attribute__ ((weak)) void HandleOtherRequests(uint8_t epNum, SetupRequest_t *req) {
    // If not implemented but received a request, trigger a breakpoint
    asm("bkpt");
}

/********************************************************************************/

void USB_OTG_SetMode(USB_OTG_Mode_t mode) {
    switch (mode) {
        case USB_MODE_DEVICE:
            MODIFY_REG(USB_OTG->GUSBCFG, USB_OTG_GUSBCFG_FHMOD | USB_OTG_GUSBCFG_FDMOD,
                    USB_OTG_GUSBCFG_FDMOD);
            while (!(USB_OTG->GUSBCFG & USB_OTG_GUSBCFG_FDMOD));
            break;

        case USB_MODE_HOST:
            MODIFY_REG(USB_OTG->GUSBCFG, USB_OTG_GUSBCFG_FHMOD | USB_OTG_GUSBCFG_FDMOD,
                    USB_OTG_GUSBCFG_FHMOD);
            while (!(USB_OTG->GUSBCFG & USB_OTG_GUSBCFG_FHMOD));
            break;

        default:
            return;
    }
}

void StallEP(uint8_t epNum) {
    // IN endpoint
    if (epNum & 0x80)
        USB_OTG_InEP(epNum & 0x7)->DIEPCTL |= USB_OTG_DIEPCTL_STALL;
    else
        USB_OTG_OutEP(epNum)->DOEPCTL |= USB_OTG_DIEPCTL_STALL;
}

static inline void USB_OTG_WaitAHBIdle(void) {
    // Wait for AHB idle state
    while (!(USB_OTG->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL));
}

void USB_OTG_FlushTXFIFO(uint8_t fifoNum) {
    USB_OTG_WaitAHBIdle();
    // TODO: reference manual also says to check that NAK Effective Interrupt flag is up
    // to ensure the core is not reading from the FIFO

    USB_OTG->GRSTCTL = USB_OTG_GRSTCTL_TXFFLSH | (fifoNum << USB_OTG_GRSTCTL_TXFNUM_Pos);
    while (USB_OTG->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH);
}

void USB_OTG_FlushRXFIFO(void) {
    USB_OTG_WaitAHBIdle();
    // TODO: reference manual also says to check that NAK Effective Interrupt flag is up
    // to ensure the core is not reading from the FIFO

    USB_OTG->GRSTCTL = USB_OTG_GRSTCTL_RXFFLSH;
    while (USB_OTG->GRSTCTL & USB_OTG_GRSTCTL_RXFFLSH);
}

// Read data into given memory buffer
void GetDataFromRxBuf(uint8_t *bufPtr, uint16_t len) {
    if (!len)
        return;
    if (!bufPtr)    // TODO: remove maybe?
        asm("bkpt");

    for (uint16_t i = 0; i < (len / 4); i++) {
        *(uint32_t*) bufPtr = USB_OTG_FIFO(0);
        bufPtr += 4;
    }

    uint8_t rem = len % 4;
    if (rem) {
        uint32_t lastData = USB_OTG_FIFO(0);
        while (rem--) {
            *bufPtr++ = (uint8_t) lastData;
            lastData >>= 8;
        }
    }
}

void LoadDataIntoTxBuf(uint8_t epNum) {
    size_t thisPacketLen = (TxDataLeft[epNum] < EP_max_packet[epNum | 0x8]) ? TxDataLeft[epNum] : EP_max_packet[epNum | 0x8];
    size_t lenWords = (thisPacketLen + 3) / 4;

    while ((USB_OTG_InEP(epNum)->DTXFSTS >= lenWords) && (TxDataLeft[epNum])) {
        thisPacketLen = (TxDataLeft[epNum] < EP_max_packet[epNum | 0x8]) ? TxDataLeft[epNum] : EP_max_packet[epNum | 0x8];
        lenWords = (thisPacketLen + 3) / 4;

        for (size_t i = 0; i < ((thisPacketLen + 3) / 4); i++) {
            USB_OTG_FIFO(epNum) = *(uint32_t*) TxData[epNum];
            TxData[epNum] += 4;
        }

        TxDataLeft[epNum] -= thisPacketLen;
        TxDataLastSent[epNum] = thisPacketLen;
    }

    // Mask TX FIFO empty interrupt if all of the data were sent
    if (!TxDataLeft[epNum])
        USB_OTG_DEV->DIEPEMPMSK &= ~(1 << epNum);
}

// Initiate transfer
inline void InitiateTransfer(uint8_t epNum, const void *data, size_t len, const void (*completeHandler)(uint8_t)) {
    epNum &= 0x7;
    TxData[epNum] = data;
    txCompleteHandler[epNum] = completeHandler;

    // Calculate number of packets
    size_t packetCount = (len + EP_max_packet[epNum | 0x8] - 1) / EP_max_packet[epNum | 0x8];

    // For endpoint 0 transmit each packet separately
    // because of stupid limitations on data length and packet count for EP0
    if (epNum == 0) {
        EP0_txPacketsLeft = (packetCount) ? packetCount - 1 : 0;

        // Calculate length of the current packet
        size_t thisPacketLen = (len < EP_max_packet[0x8]) ? len : EP_max_packet[0x8];

        MODIFY_REG(USB_OTG_InEP(epNum)->DIEPTSIZ, USB_OTG_DIEPTSIZ_PKTCNT, 1 << USB_OTG_DIEPTSIZ_PKTCNT_Pos);
        MODIFY_REG(USB_OTG_InEP(epNum)->DIEPTSIZ, USB_OTG_DIEPTSIZ_XFRSIZ, thisPacketLen);

        // This is needed to prevent loading more than one packet of data into TX FIFO at a time
        EP0_txDataLeft = len - thisPacketLen;
        TxDataLeft[0] = thisPacketLen;
    }
    // For all other endpoints load packet count and data length directly into endpoint register
    else {
        MODIFY_REG(USB_OTG_InEP(epNum)->DIEPTSIZ, USB_OTG_DIEPTSIZ_PKTCNT, packetCount << USB_OTG_DIEPTSIZ_PKTCNT_Pos);
        MODIFY_REG(USB_OTG_InEP(epNum)->DIEPTSIZ, USB_OTG_DIEPTSIZ_XFRSIZ, len);

        TxDataLeft[epNum] = len;
    }

    USB_OTG_InEP(epNum)->DIEPCTL |= USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA;

    USB_OTG_DEV->DIEPEMPMSK |= 1 << epNum;
}

// Initiate reception
inline void InitiateReception(uint8_t epNum, void *bufPtr, size_t len, const void (*completeHandler)(uint8_t, void*, size_t)) {
    epNum &= 0x7;
    RxData[epNum] = bufPtr;
    rxCompleteHandler[epNum] = completeHandler;
    RxDataLeft[epNum] = len;

    size_t packetCount = (len + EP_max_packet[epNum] - 1) / EP_max_packet[epNum];

    MODIFY_REG(USB_OTG_OutEP(epNum)->DOEPTSIZ, USB_OTG_DOEPTSIZ_PKTCNT, packetCount << USB_OTG_DOEPTSIZ_PKTCNT_Pos);
    MODIFY_REG(USB_OTG_OutEP(epNum)->DOEPTSIZ, USB_OTG_DOEPTSIZ_XFRSIZ, len);

    USB_OTG_OutEP(epNum)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK | USB_OTG_DOEPCTL_EPENA;
}

uint8_t txBusy(uint8_t epNum) {
    if (TxData[epNum])
        return 1;
    else
        return 0;
}

uint8_t rxBusy(uint8_t epNum) {
    if (RxData[epNum])
        return 1;
    else
        return 0;
}

void StateStageIn(uint8_t epNum, void *_, size_t __) {
    InitiateTransfer(epNum, NULL, 0, 0);
}

void StateStageOut(uint8_t epNum) {
    InitiateReception(epNum, NULL, 0, 0);
}
/********************************************************************************/

void USB_init(void) {
    // Enable AHB clock for USB
    RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
    // Reset USB peripheral
    USB_OTG->GRSTCTL |= USB_OTG_GRSTCTL_CSRST;
    while (USB_OTG->GRSTCTL & USB_OTG_GRSTCTL_CSRST);
    // Turn on USB transceiver
    USB_OTG->GCCFG |= USB_OTG_GCCFG_PWRDWN;
    // Disable VBUS sensing
    MODIFY_REG(USB_OTG->GCCFG, (USB_OTG_GCCFG_NOVBUSSENS | USB_OTG_GCCFG_VBUSBSEN | USB_OTG_GCCFG_VBUSASEN),
            USB_OTG_GCCFG_NOVBUSSENS);
    // Restart the PHY clock
    USB_OTG_PCGCCTL = 0U;
    // Set device mode
    USB_OTG_SetMode(USB_MODE_DEVICE);

    // Set device speed to full-speed
    MODIFY_REG(USB_OTG_DEV->DCFG, USB_OTG_DCFG_DSPD, 3 << USB_OTG_DCFG_DSPD_Pos);

    // RX buffer
    USB_OTG->GRXFSIZ = 128;
    // TX buffer 0
    USB_OTG->DIEPTXF0_HNPTXFSIZ = (64 << USB_OTG_DIEPTXF_INEPTXFD_Pos) |
            128;
    // TX buffer 1
    USB_OTG->DIEPTXF[0] = (64 << USB_OTG_DIEPTXF_INEPTXFD_Pos) |
            192;
    // TX buffer 2
    USB_OTG->DIEPTXF[1] = (64 << USB_OTG_DIEPTXF_INEPTXFD_Pos) |
            256;

    // Enable interrupts
    USB_OTG->GINTMSK |= USB_OTG_GINTMSK_USBRST | USB_OTG_GINTMSK_ENUMDNEM
            | USB_OTG_GINTMSK_IEPINT | USB_OTG_GINTMSK_OEPINT
            | USB_OTG_GINTMSK_RXFLVLM | USB_OTG_GINTMSK_SOFM;
    // Clear pending interrupts
    USB_OTG->GINTSTS = 0xFFFFFFFF; // 0xBFFFFFF
    // Enable USB interrupts
    USB_OTG->GAHBCFG |= USB_OTG_GAHBCFG_GINT;
    NVIC_EnableIRQ(OTG_FS_IRQn);

    // Release soft disconnect
    USB_OTG_DEV->DCTL &= ~USB_OTG_DCTL_SDIS;
}

/********************************************************************************/

/********************** MAIN USB INTERRUPT HANDLER ******************************/
void USB_IRQ_Handler(void) {

    if (!(USB_OTG->GINTSTS & USB_OTG->GINTMSK)) {
        return;
    }

    static SetupRequest_t lastRequest;

    // RX FIFO not empty interrupt
    if (USB_OTG->GINTSTS & USB_OTG_GINTSTS_RXFLVL) {
        uint32_t packetInfo = USB_OTG->GRXSTSP;

        USB_OTG->GINTMSK &= ~USB_OTG_GINTMSK_RXFLVLM;
        //uint8_t epNum = packetInfo & USB_OTG_GRXSTSP_EPNUM;

        if ((packetInfo & USB_OTG_GRXSTSP_PKTSTS) == USB_OTG_GRXSTSP_PKTSTS_DATA) {
            size_t receivedCnt = (packetInfo & USB_OTG_GRXSTSP_BCNT) >> USB_OTG_GRXSTSP_BCNT_Pos;
            size_t epNum = packetInfo & USB_OTG_GRXSTSP_EPNUM;
            receivedCnt = (RxDataLeft[epNum] < receivedCnt) ? RxDataLeft[epNum] : receivedCnt;

            GetDataFromRxBuf(RxData[epNum], receivedCnt);
            RxData[epNum] += receivedCnt;
            RxDataLeft[epNum] -= receivedCnt;
            RxDataReceived[epNum] += receivedCnt;
            // TODO maybe add error status in case of buffer overflow (received more than needed)
        }
        else if ((packetInfo & USB_OTG_GRXSTSP_PKTSTS) == USB_OTG_GRXSTSP_PKTSTS_SETUP) {
            GetDataFromRxBuf((uint8_t*) &lastRequest, sizeof(SetupRequest_t));
        }

        USB_OTG->GINTMSK |= USB_OTG_GINTMSK_RXFLVLM;
    }

    if (USB_OTG->GINTSTS & USB_OTG_GINTSTS_OEPINT) {
        uint32_t allEpInt = USB_OTG_AllOutEPInterrupts;
        for (uint8_t epNum = 0; epNum < USB_OTG_HW_EP_NUM; epNum++) {
            // If this EP has pending interrupts
            if (allEpInt & 0x1) {
                uint32_t epInt = USB_OTG_OutEPInterrupts(epNum);
                // Transfer completed interrupt
                if (epInt & USB_OTG_DIEPINT_XFRC) {
                    // Clear XFRC interrupt bit by writing 1
                    USB_OTG_OutEP(epNum)->DOEPINT = USB_OTG_DOEPINT_XFRC;

                    uint8_t *bufPtr = RxData[epNum] - RxDataReceived[epNum];
                    size_t nBytes = RxDataReceived[epNum];
                    void (*handler)(uint8_t, void*, size_t) = rxCompleteHandler[epNum];

                    RxData[epNum] = NULL;
                    RxDataReceived[epNum] = 0;
                    rxCompleteHandler[epNum] = NULL;

                    if (handler)
                        handler(epNum, bufPtr, nBytes);
                }
                // Setup stage done interrupt
                if (epInt & USB_OTG_DOEPINT_STUP) {
                    // Clear STUP interrupt bit by writing 1
                    USB_OTG_OutEP(epNum)->DOEPINT = USB_OTG_DOEPINT_STUP;

                    if (lastRequest.type == REQUEST_TYPE_STANDARD)
                        // TODO: here we use epNum of the ep that called the interrupt but use
                        // a singualr lastRequest. Maybe we should make a setup request field for
                        // each endpoint just in case (can be done at the same time as the endpoint
                        // structure)
                        HandleStandardRequest(epNum, &lastRequest);
                    else
                        HandleOtherRequests(epNum, &lastRequest);

                    // Change the address if required
                    if (addressToSet != 0xFF) {
                        USB_OTG_DEV->DCFG &= ~(USB_OTG_DCFG_DAD);
                        USB_OTG_DEV->DCFG |= ((uint32_t) addressToSet << 4) & USB_OTG_DCFG_DAD;
                        //MODIFY_REG(USB_OTG_DEV->DCFG, USB_OTG_DCFG_DAD, (addressToSet << USB_OTG_DCFG_DAD_Pos));
                        if (!addressToSet)
                            connState = CONN_STATE_DEFAULT;
                        else
                            connState = CONN_STATE_ADDRESS;
                        addressToSet = 0xFF;
                    }
                }

                if (epInt & USB_OTG_DOEPINT_OTEPDIS)
                    USB_OTG_OutEP(epNum)->DOEPINT = USB_OTG_DOEPINT_OTEPDIS;

                if (epInt & USB_OTG_DOEPINT_EPDISD) {
                    if (USB_OTG->GINTSTS & USB_OTG_GINTSTS_BOUTNAKEFF)
                        USB_OTG_DEV->DCTL |= USB_OTG_DCTL_CGONAK;
                    USB_OTG_OutEP(epNum)->DOEPINT = USB_OTG_DOEPINT_EPDISD;
                }

                if (epInt & USB_OTG_DOEPINT_OTEPSPR)
                    USB_OTG_OutEP(epNum)->DOEPINT = USB_OTG_DOEPINT_OTEPSPR;

                if (epInt & USB_OTG_DOEPINT_NAK)
                    USB_OTG_OutEP(epNum)->DOEPINT = USB_OTG_DOEPINT_NAK;
                // TODO: process all other interrupts
            }
            allEpInt >>= 1;
        }

    }
    // IN Endpoint interrupt
    if (USB_OTG->GINTSTS & USB_OTG_GINTSTS_IEPINT) {
        uint32_t allEpInt = USB_OTG_AllInEPInterrupts;

        for (uint8_t epNum = 0; epNum < USB_OTG_HW_EP_NUM; epNum++) {
            // If this EP has pending interrupts
            if (allEpInt & 0x1) {
                uint32_t epInt = USB_OTG_InEPInterrupts(epNum);
                // Transfer completed interrupt
                if (epInt & USB_OTG_DIEPINT_XFRC) {
                    // Mask TX FIFO empty interrupt
                    USB_OTG_DEV->DIEPEMPMSK &= ~(1 << epNum);
                    // Clear XFRC interrupt bit by writing 1
                    USB_OTG_InEP(epNum)->DIEPINT = USB_OTG_DIEPINT_XFRC;

                    // Handle EP0 multipacket transmission
                    if (epNum == 0 && EP0_txPacketsLeft) {
                        EP0_txPacketsLeft--;

                        // Calculate current packet length
                        size_t thisPacketLen = (EP0_txDataLeft < EP_max_packet[0x8]) ? EP0_txDataLeft : EP_max_packet[0x8];
                        // Load packet info into EP0 registers
                        MODIFY_REG(USB_OTG_InEP(epNum)->DIEPTSIZ, USB_OTG_DIEPTSIZ_PKTCNT, 1 << USB_OTG_DIEPTSIZ_PKTCNT_Pos);
                        MODIFY_REG(USB_OTG_InEP(epNum)->DIEPTSIZ, USB_OTG_DIEPTSIZ_XFRSIZ, thisPacketLen);
                        // Open the endpoint
                        USB_OTG_InEP(epNum)->DIEPCTL |= USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA;
                        USB_OTG_DEV->DIEPEMPMSK |= 1 << epNum;
                        // Update variables
                        TxDataLastSent[0] = thisPacketLen;
                        TxDataLeft[0] = thisPacketLen;
                        EP0_txDataLeft -= thisPacketLen;
                    }
                    else {
                        // This is stupid! The USB_OTG does not send XFRC interrupt if the packet sent had zero length.
                        // This makes it really hard to do stuff on order as there is no way to call the complete handler
                        // AFTER the ZLP has been sent.
                        // This probably won't be an issue as long as the USB_OTG can process two transfer initiations back-to-back
                        // in case the txCompleteHandler() calls InitiateTransfer() right after the core has requested a ZLP to be sent
                        // In my testing with virtual COM port device this in fact works just fine and both the ZLP and the data
                        // are being correctly sent.

                        // Edit: I think I've fixed this completely by preventing the ZLP from being sent if the txCompleteHandler() starts a new transmission
                        void (*handler)(uint8_t) = txCompleteHandler[epNum];

                        TxData[epNum] = NULL;
                        txCompleteHandler[epNum] = NULL;

                        // Call the handler first, then check if a ZLP is needed
                        if (handler)
                        handler(epNum);
                        // If the last packet had maximum length and the txCompletehandler() didn't start a new transfer, send a zero length packet
                        if (!TxData[epNum] && TxDataLastSent[epNum] == EP_max_packet[epNum | 0x8]) {
                            TxDataLastSent[epNum] = 0;
                            InitiateTransfer(epNum, NULL, 0, txCompleteHandler[epNum]);
                        }
                    }
                }
                // TX FIFO empty interrupt
                if (epInt & USB_OTG_DIEPINT_TXFE) {
                    LoadDataIntoTxBuf(epNum);
                }
                // TODO: process all other interrupts
                if (epInt & USB_OTG_DIEPINT_TOC)
                    USB_OTG_InEP(epNum)->DIEPINT = USB_OTG_DIEPINT_TOC;
                if (epInt & USB_OTG_DIEPINT_ITTXFE)
                    USB_OTG_InEP(epNum)->DIEPINT = USB_OTG_DIEPINT_ITTXFE;
                if (epInt & USB_OTG_DIEPINT_INEPNE)
                    USB_OTG_InEP(epNum)->DIEPINT = USB_OTG_DIEPINT_INEPNE;
                if (epInt & USB_OTG_DIEPINT_EPDISD) {
                    USB_OTG_FlushTXFIFO(epNum);
                    USB_OTG_InEP(epNum)->DIEPINT = USB_OTG_DIEPINT_EPDISD;
                }
            }
            allEpInt >>= 1;
        }
    }
    // Reset interrupt
    if (USB_OTG->GINTSTS & USB_OTG_GINTSTS_USBRST) {
        USB_OTG_FlushTXFIFO(0x10);
        USB_OTG_FlushRXFIFO();

        for (uint8_t i = 0; i < USB_OTG_HW_EP_NUM; i++) {
            USB_OTG_InEP(i)->DIEPINT = 0xFFFF;    // 0xFB7FU;
            USB_OTG_InEP(i)->DIEPCTL &= ~USB_OTG_DIEPCTL_STALL;

            USB_OTG_OutEP(i)->DOEPINT = 0xFFFF;    // 0xFB7FU;
            USB_OTG_OutEP(i)->DOEPCTL &= ~USB_OTG_DOEPCTL_STALL;
            USB_OTG_OutEP(i)->DOEPCTL |= USB_OTG_DOEPCTL_SNAK;
        }

        // TODO: make tx fifo allocation functuion
        MODIFY_REG(USB_OTG_InEP(0)->DIEPCTL, USB_OTG_DIEPCTL_TXFNUM, (0 << USB_OTG_DIEPCTL_TXFNUM_Pos));

        // Unmask EP0 interrupts
        USB_OTG_DEV->DAINTMSK |= (0b1 << USB_OTG_DAINTMSK_IEPM_Pos) | (0b1 << USB_OTG_DAINTMSK_OEPM_Pos);
        // Unmask all endpoint-specific interrupt events
        USB_OTG_DEV->DOEPMSK |= USB_OTG_DOEPMSK_STUPM
                | USB_OTG_DOEPMSK_XFRCM
                | USB_OTG_DOEPMSK_EPDM
                | USB_OTG_DOEPMSK_OTEPSPRM
                | USB_OTG_DOEPMSK_NAKM;

        USB_OTG_DEV->DIEPMSK |= USB_OTG_DIEPMSK_TOM
                | USB_OTG_DIEPMSK_XFRCM
                | USB_OTG_DIEPMSK_EPDM;

        // Set address to 0
        USB_OTG_DEV->DCFG &= ~USB_OTG_DCFG_DAD;

        // Open endpoint 0
        // Set max packet size to 64 bytes
        MODIFY_REG(USB_OTG_InEP(0U)->DIEPCTL, USB_OTG_DIEPCTL_MPSIZ, 0 << USB_OTG_DIEPCTL_MPSIZ_Pos);
        EP_max_packet[0 | 0x8] = 64;
        MODIFY_REG(USB_OTG_OutEP(0U)->DOEPCTL, USB_OTG_DOEPCTL_MPSIZ, 0 << USB_OTG_DOEPCTL_MPSIZ_Pos);
        EP_max_packet[0] = 64;

        // TODO: switch to a function here - this is endpoint initialisation for reception
        USB_OTG_OutEP(0)->DOEPTSIZ = (3 << USB_OTG_DOEPTSIZ_STUPCNT_Pos)
                | (1 << USB_OTG_DOEPTSIZ_PKTCNT_Pos) | 3 * 8 /* maybe 3*8 is better */;
        USB_OTG_OutEP(0)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA;        // ?

        // Clear global IN NAK
        USB_OTG_DEV->DCTL |= USB_OTG_DCTL_CGINAK;

        connState = CONN_STATE_DEFAULT;

        USB_ResetHandler();

        USB_OTG->GINTSTS = USB_OTG_GINTSTS_USBRST;
    }

    // Enumeration done interrupt
    if (USB_OTG->GINTSTS & USB_OTG_GINTSTS_ENUMDNE) {
        USB_OTG->GINTSTS = USB_OTG_GINTSTS_ENUMDNE;
    }

    // StartOfFrame interrupt
    if (USB_OTG->GINTSTS & USB_OTG_GINTSTS_SOF) {
        USB_StartOfFrameHandler();
        USB_OTG->GINTSTS = USB_OTG_GINTSTS_SOF;
    }
}
/********************************************************************************/
