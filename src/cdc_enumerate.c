/* ----------------------------------------------------------------------------
 *         SAM Software Package License
 * ----------------------------------------------------------------------------
 * Copyright (c) 2011-2014, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following condition is met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

#include "uf2.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

PacketBuffer ctrlOutCache;
PacketBuffer endpointCache[MAX_EP];

__attribute__((__aligned__(4))) UsbDeviceDescriptor usb_endpoint_table[MAX_EP];

__attribute__((__aligned__(4)))
const char devDescriptor[] = {
    /* Device descriptor */
    0x12, // bLength
    0x01, // bDescriptorType
// bcdUSBL - v2.00; v2.10 is needed for WebUSB; there were issues with newer laptops running Win10
// but it seems to be resolved
#if USE_WEBUSB
    0x10,
#else
    0x00,
#endif
    0x02,           //
    0xEF,           // bDeviceClass:    Misc
    0x02,           // bDeviceSubclass:
    0x01,           // bDeviceProtocol:
    0x40,           // bMaxPacketSize0
    USB_VID & 0xff, // vendor ID
    USB_VID >> 8,   //
    USB_PID & 0xff, // product ID
    USB_PID >> 8,   //
    0x01,           // bcdDeviceL
    0x42,           //
    0x01,           // iManufacturer    // 0x01
    0x02,           // iProduct
    0x03,           // SerialNumber (required (!) for WebUSB)
    0x01            // bNumConfigs
};

#define CFG_DESC_SIZE (32 + USE_CDC * (58 + 8) + USE_HID * 32 + USE_WEBUSB * 23)
#define HID_IF_NUM (USE_CDC ? 3 : 1)
#define WEB_IF_NUM (HID_IF_NUM + 1)

#if USE_HID
// can be requested separately from the entire config desc
__attribute__((__aligned__(4)))
char hidCfgDescriptor[] = {
    9,          // size
    4,          // interface
    HID_IF_NUM, // interface number
    0,          // alternate
    2,          // num. endpoints
    0x03,       // HID
    0,          // sub
    0,          // sub
    3,          // stringID
};

__attribute__((__aligned__(4)))
const char hidDescriptor[] = {
    0x06, 0x97, 0xFF, // usage page vendor 0x97 (usage 0xff97 0x0001)
    0x09, 0x01,       // usage 1
    0xA1, 0x01,       // collection - application
    0x15, 0x00,       // logical min 0
    0x26, 0xFF, 0x00, // logical max 255
    0x75, 8,          // report size 8
    0x95, 64,         // report count 64
    0x09, 0x01,       // usage 1
    0x81, 0x02,       // input: data, variable, absolute
    0x95, 64,         // report count 64
    0x09, 0x01,       // usage 1
    0x91, 0x02,       // output: data, variable, absolute
    0x95, 1,          // report count 1
    0x09, 0x01,       // usage 1
    0xB1, 0x02,       // feature: data, variable, absolute
    0xC0,             // end
};
#endif

#ifndef USB_POWER_MA
#define USB_POWER_MA 500
#endif
__attribute__((__aligned__(4)))
char cfgDescriptor[] = {
    /* ============== CONFIGURATION 1 =========== */
    /* Configuration 1 descriptor */
    0x09,          // CbLength
    0x02,          // CbDescriptorType
    CFG_DESC_SIZE, // CwTotalLength 2 EP + Control
    0x00,
    1 + 2 * USE_CDC + USE_HID + USE_WEBUSB, // CbNumInterfaces
    0x01,                                   // CbConfigurationValue
    0x00,                                   // CiConfiguration
    0x80,                                   // CbmAttributes 0x80 - bus-powered
    USB_POWER_MA/2,                         // MaxPower (*2mA)

#if USE_CDC
    // IAD for CDC
    0x08, // bLength;
    0x0B, // bDescriptorType;
    0x00, // bFirstInterface;
    0x02, // bInterfaceCount;
    0x02, // bFunctionClass;
    0x02, // bFunctionSubclass;
    0x01, // bFunctionProtocol;
    0x00, // iFunction; string

    /* Communication Class Interface Descriptor Requirement */
    0x09, // bLength
    0x04, // bDescriptorType
    0x00, // bInterfaceNumber
    0x00, // bAlternateSetting
    0x01, // bNumEndpoints
    0x02, // bInterfaceClass
    0x02, // bInterfaceSubclass
    0x01, // bInterfaceProtocol
    0x00, // iInterface

    /* Header Functional Descriptor */
    0x05, // bFunction Length
    0x24, // bDescriptor type: CS_INTERFACE
    0x00, // bDescriptor subtype: Header Func Desc
    0x10, // bcdCDC:1.1
    0x01,

    /* ACM Functional Descriptor */
    0x04, // bFunctionLength
    0x24, // bDescriptor Type: CS_INTERFACE
    0x02, // bDescriptor Subtype: ACM Func Desc
    0x06, // bmCapabilities

    /* Union Functional Descriptor */
    0x05, // bFunctionLength
    0x24, // bDescriptorType: CS_INTERFACE
    0x06, // bDescriptor Subtype: Union Func Desc
    0x00, // bMasterInterface: Communication Class Interface
    0x01, // bSlaveInterface0: Data Class Interface

    /* Call Management Functional Descriptor */
    0x05, // bFunctionLength
    0x24, // bDescriptor Type: CS_INTERFACE
    0x01, // bDescriptor Subtype: Call Management Func Desc
    0x03, // bmCapabilities: D1 + D0
    0x01, // bDataInterface: Data Class Interface 1

    /* Endpoint 1 descriptor */
    0x07, // bLength
    0x05, // bDescriptorType
    USB_EP_COMM | 0x80, // bEndpointAddress, Endpoint 03 - IN
    0x03, // bmAttributes      INT
    0x08, // wMaxPacketSize
    0x00,
    0xFF, // bInterval

    /* Data Class Interface Descriptor Requirement */
    0x09, // bLength
    0x04, // bDescriptorType
    0x01, // bInterfaceNumber
    0x00, // bAlternateSetting
    0x02, // bNumEndpoints
    0x0A, // bInterfaceClass
    0x00, // bInterfaceSubclass
    0x00, // bInterfaceProtocol
    0x00, // iInterface

    /* First alternate setting */
    /* Endpoint 1 descriptor */
    0x07,     // bLength
    0x05,     // bDescriptorType
    USB_EP_IN | 0x80, // bEndpointAddress, Endpoint 01 - IN
    0x02,     // bmAttributes      BULK
    PKT_SIZE, // wMaxPacketSize
    0x00,
    0x00, // bInterval

    /* Endpoint 2 descriptor */
    0x07,     // bLength
    0x05,     // bDescriptorType
    USB_EP_OUT, // bEndpointAddress, Endpoint 02 - OUT
    0x02,     // bmAttributes      BULK
    PKT_SIZE, // wMaxPacketSize
    0x00,
    0x00, // bInterval
#endif

    // MSC

    9,               /// descriptor size in bytes
    4,               /// descriptor type - interface
    USE_CDC ? 2 : 0, /// interface number
    0,               /// alternate setting number
    2,               /// number of endpoints
    USE_MSC * 8,     /// class code - mass storage
    6,               /// subclass code - SCSI transparent command set
    80,              /// protocol code - bulk only transport
    0,               /// interface string index

    7,                    /// descriptor size in bytes
    5,                    /// descriptor type - endpoint
    USB_EP_MSC_IN | 0x80, /// endpoint direction and number - in, 2
    2,                    /// transfer type - bulk
    PKT_SIZE,             /// maximum packet size
    0,
    0, /// not used

    7,              /// descriptor size in bytes
    5,              /// descriptor type - endpoint
    USB_EP_MSC_OUT, /// endpoint direction and number - out, 1
    2,              /// transfer type - bulk
    PKT_SIZE,       /// maximum packet size
    0,
    0, /// maximum NAK rate

#if USE_HID
    // HID
    9,          // size
    4,          // interface
    HID_IF_NUM, // interface number
    0,          // alternate
    2,          // num. endpoints
    0x03,       // HID
    0,          // sub
    0,          // sub
    0,          // stringID

    9,
    0x21,                     // HID
    0x00, 0x01,               // hidbcd 1.00
    0x00,                     // country code
    0x01,                     // num desc
    0x22,                     // report desc type
    sizeof(hidDescriptor), 0, // size of report

    // interrupt endpoints with interval=1
    7, 5, 0x80 | USB_EP_HID, 3, PKT_SIZE, 0, 1, // in
    7, 5, USB_EP_HID, 3, PKT_SIZE, 0, 1,        // out
#endif

#if USE_WEBUSB
    9,          // size
    4,          // interface
    WEB_IF_NUM, // interface number
    0,          // alternate
    2,          // num. endpoints
    0xFF,       // Vendor
    42,         // sub
    1,          // sub
    0,          // stringID

    // interrupt endpoints with interval=1
    7, 5, 0x80 | USB_EP_WEB, 3, PKT_SIZE, 0, 1, // in
    7, 5, USB_EP_WEB, 3, PKT_SIZE, 0, 1,        // out
#endif
};

#define WINUSB_SIZE 170

__attribute__((__aligned__(4)))
static char bosDescriptor[] = {
    0x05, // Length
    0x0F, // Binary Object Store descriptor
#if USE_WEBUSB
    0x39, 0x00, // Total length
    0x02,       // Number of device capabilities
#else
    0x05, 0x00, // Length
    0x00        // num caps
#endif

#if USE_WEBUSB
    // WebUSB Platform Capability descriptor (bVendorCode == 0x01).
    0x18,                                           // Length
    0x10,                                           // Device Capability descriptor
    0x05,                                           // Platform Capability descriptor
    0x00,                                           // Reserved
    0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47, // WebUSB GUID
    0x8B, 0xFD, 0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65, // WebUSB GUID
    0x00, 0x01,                                     // Version 1.0
    0x01,                                           // Vendor request code
    0x00,                                           // landing page

    0x1C,                                           // Length
    0x10,                                           // Device Capability descriptor
    0x05,                                           // Platform Capability descriptor
    0x00,                                           // Reserved
    0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C, // MS OS 2.0 GUID
    0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F, // MS OS 2.0 GUID
    0x00, 0x00, 0x03, 0x06,                         // Windows version
    WINUSB_SIZE, 0x00,                              // Descriptor set length
    0x02,                                           // Vendor request code
    0x00                                            // Alternate enumeration code
#endif
};

#if USE_WEBUSB
__attribute__((__aligned__(4)))
static char msOS20Descriptor[] = {
    // Microsoft OS 2.0 descriptor set header (table 10)
    0x0A, 0x00,             // Descriptor size (10 bytes)
    0x00, 0x00,             // MS OS 2.0 descriptor set header
    0x00, 0x00, 0x03, 0x06, // Windows version (8.1) (0x06030000)
    WINUSB_SIZE, 0x00,      // Size, MS OS 2.0 descriptor set

    // Microsoft OS 2.0 function subset header
    0x08, 0x00,             // Descriptor size (8 bytes)
    0x02, 0x00,             // MS OS 2.0 function subset header
    WEB_IF_NUM,             // first interface no
    0x00,                   // Reserved
    WINUSB_SIZE - 10, 0x00, // Size, MS OS 2.0 function subset

    // Microsoft OS 2.0 compatible ID descriptor (table 13)
    20, 0x00,                     // wLength
    0x03, 0x00,                   // MS_OS_20_FEATURE_COMPATIBLE_ID
    'W', 'I', 'N', 'U', 'S', 'B', //
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // interface guids
    132, 0, 4, 0, 7, 0,
    //
    42, 0,
    //
    'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0, 'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'f', 0,
    'a', 0, 'c', 0, 'e', 0, 'G', 0, 'U', 0, 'I', 0, 'D', 0, 's', 0, 0, 0,
    //
    80, 0,
    //
    '{', 0, '9', 0, '2', 0, 'C', 0, 'E', 0, '6', 0, '4', 0, '6', 0, '2', 0, '-', 0, '9', 0, 'C', 0,
    '7', 0, '7', 0, '-', 0, '4', 0, '6', 0, 'F', 0, 'E', 0, '-', 0, '9', 0, '3', 0, '3', 0, 'B', 0,
    '-', 0, '3', 0, '1', 0, 'C', 0, 'B', 0, '9', 0, 'C', 0, '5', 0, 'B', 0, 'B', 0, '3', 0, 'B', 0,
    '9', 0, '}', 0, 0, 0, 0, 0};

STATIC_ASSERT(sizeof(msOS20Descriptor) == WINUSB_SIZE);

#endif

static uint8_t currentConfiguration;

typedef struct {
    uint8_t len;
    uint8_t type;
    uint8_t data[70];
} StringDescriptor;


// Serial numbers are derived from four 32-bit words. Add one character for null terminator
#define SERIAL_NUMBER_LENGTH (4 * 8 + 1)
// serial_number will be filled in when needed.
static char serial_number[SERIAL_NUMBER_LENGTH];

static const char *string_descriptors[] = {0, VENDOR_NAME, PRODUCT_NAME, serial_number};
#define STRING_DESCRIPTOR_COUNT (sizeof(string_descriptors) / sizeof(string_descriptors[0]))

static void load_serial_number(char serial_number[SERIAL_NUMBER_LENGTH]) {
    // These are locations that taken together make up a unique serial number.
    #ifdef SAMD21
    uint32_t* addresses[4] = {(uint32_t *) 0x0080A00C, (uint32_t *) 0x0080A040,
                              (uint32_t *) 0x0080A044, (uint32_t *) 0x0080A048};
    #endif
    #ifdef SAML21
    uint32_t* addresses[4] = {(uint32_t *) 0x0080A00C, (uint32_t *) 0x0080A040,
                              (uint32_t *) 0x0080A044, (uint32_t *) 0x0080A048};
    #endif
    #ifdef SAMD51
    uint32_t* addresses[4] = {(uint32_t *) 0x008061FC, (uint32_t *) 0x00806010,
                              (uint32_t *) 0x00806014, (uint32_t *) 0x00806018};
    #endif
    uint32_t serial_number_idx = 0;
    for (int i = 0; i < 4; i++) {
        serial_number_idx += writeNum(&(serial_number[serial_number_idx]), *(addresses[i]), true);
    }
    serial_number[serial_number_idx] = '\0';
}

#if USE_CDC
static usb_cdc_line_coding_t line_coding = {
    115200, // baudrate
    0,      // 1 Stop Bit
    0,      // None Parity
    8       // 8 Data bits
};

static USB_CDC pCdc;
#endif

/* USB standard request code */
#define STD_GET_STATUS_ZERO 0x0080
#define STD_GET_STATUS_INTERFACE 0x0081
#define STD_GET_STATUS_ENDPOINT 0x0082

#define STD_CLEAR_FEATURE_ZERO 0x0100
#define STD_CLEAR_FEATURE_INTERFACE 0x0101
#define STD_CLEAR_FEATURE_ENDPOINT 0x0102
#define STD_VENDOR_CTRL1 0x01C0
#define STD_VENDOR_CTRL2 0x02C0

#define STD_SET_FEATURE_ZERO 0x0300
#define STD_SET_FEATURE_INTERFACE 0x0301
#define STD_SET_FEATURE_ENDPOINT 0x0302

#define STD_SET_ADDRESS 0x0500
#define STD_GET_DESCRIPTOR 0x0680
#define STD_GET_DESCRIPTOR1 0x0681
#define STD_SET_DESCRIPTOR 0x0700
#define STD_GET_CONFIGURATION 0x0880
#define STD_SET_CONFIGURATION 0x0900
#define STD_GET_INTERFACE 0x0A81
#define STD_SET_INTERFACE 0x0B01
#define STD_SYNCH_FRAME 0x0C82

/* CDC Class Specific Request Code */
#define GET_LINE_CODING 0x21A1
#define SET_LINE_CODING 0x2021
#define SET_CONTROL_LINE_STATE 0x2221

// MSC
#define MSC_RESET 0xFF21
#define MSC_GET_MAX_LUN 0xFEA1

// HID
#define HID_REQUEST_GET_REPORT 0x01A1
#define HID_REQUEST_GET_IDLE 0x02A1
#define HID_REQUEST_GET_PROTOCOL 0x03A1
#define HID_REQUEST_SET_REPORT 0x0921
#define HID_REQUEST_SET_IDLE 0x0A21
#define HID_REQUEST_SET_PROTOCOL 0x0B21

static void AT91F_CDC_Enumerate(void);

STATIC_ASSERT(sizeof(cfgDescriptor) == CFG_DESC_SIZE);

/**
 * \fn AT91F_InitUSB
 *
 * \brief Initializes USB
 */
void AT91F_InitUSB(void) {
    uint32_t pad_transn, pad_transp, pad_trim;

    /* Enable USB clock */
    #ifdef SAMD21
    PM->APBBMASK.reg |= PM_APBBMASK_USB;
    #define DM_PIN PIN_PA24G_USB_DM
    #define DM_MUX MUX_PA24G_USB_DM
    #define DP_PIN PIN_PA25G_USB_DP
    #define DP_MUX MUX_PA25G_USB_DP
    #endif
    #ifdef SAML21
    MCLK->APBBMASK.reg |= MCLK_APBBMASK_USB;
    #define DM_PIN PIN_PA24G_USB_DM
    #define DM_MUX MUX_PA24G_USB_DM
    #define DP_PIN PIN_PA25G_USB_DP
    #define DP_MUX MUX_PA25G_USB_DP
    #endif
    #ifdef SAMD51
    #define DM_PIN PIN_PA24H_USB_DM
    #define DM_MUX MUX_PA24H_USB_DM
    #define DP_PIN PIN_PA25H_USB_DP
    #define DP_MUX MUX_PA25H_USB_DP
    #endif

    /* Set up the USB DP/DN pins */
    PORT->Group[0].PINCFG[DM_PIN].bit.PMUXEN = 1;
    PORT->Group[0].PMUX[DM_PIN / 2].reg &= ~(0xF << (4 * (DM_PIN & 0x01u)));
    PORT->Group[0].PMUX[DM_PIN / 2].reg |= DM_MUX << (4 * (DM_PIN & 0x01u));
    PORT->Group[0].PINCFG[DP_PIN].bit.PMUXEN = 1;
    PORT->Group[0].PMUX[DP_PIN / 2].reg &= ~(0xF << (4 * (DP_PIN & 0x01u)));
    PORT->Group[0].PMUX[DP_PIN / 2].reg |= DP_MUX << (4 * (DP_PIN & 0x01u));

    #ifdef SAMD21
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(6) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
    while (GCLK->STATUS.bit.SYNCBUSY) {}
    #endif
    #ifdef SAML21
    GCLK->PCHCTRL[USB_GCLK_ID].reg = ( GCLK_PCHCTRL_CHEN | GCLK_PCHCTRL_GEN_GCLK0 );
    while ( (GCLK->PCHCTRL[USB_GCLK_ID].reg & GCLK_PCHCTRL_CHEN) == 0 );        // wait for sync
    #endif
    #ifdef SAMD51
    GCLK->PCHCTRL[USB_GCLK_ID].reg = GCLK_PCHCTRL_GEN_GCLK0_Val | (1 << GCLK_PCHCTRL_CHEN_Pos);
    MCLK->AHBMASK.bit.USB_ = true;
    MCLK->APBBMASK.bit.USB_ = true;

    while(GCLK->SYNCBUSY.bit.GENCTRL0) {}
    #endif

    /* Reset */
    USB->HOST.CTRLA.bit.SWRST = 1;
    while (USB->HOST.SYNCBUSY.bit.SWRST) {
        /* Sync wait */
    }

    /* Load Pad Calibration */
    pad_transn = ((*((uint32_t*) USB_FUSES_TRANSN_ADDR)) & USB_FUSES_TRANSN_Msk) >> USB_FUSES_TRANSN_Pos;

    if (pad_transn == 0x1F) {
        pad_transn = 5;
    }

    USB->HOST.PADCAL.bit.TRANSN = pad_transn;

    pad_transp = ((*((uint32_t*) USB_FUSES_TRANSP_ADDR)) & USB_FUSES_TRANSP_Msk) >> USB_FUSES_TRANSP_Pos;

    if (pad_transp == 0x1F) {
        pad_transp = 29;
    }

    USB->HOST.PADCAL.bit.TRANSP = pad_transp;
    pad_trim = ((*((uint32_t*) USB_FUSES_TRIM_ADDR)) & USB_FUSES_TRIM_Msk) >> USB_FUSES_TRIM_Pos;

    if (pad_trim == 0x7) {
        pad_trim = 3;
    }

    USB->HOST.PADCAL.bit.TRIM = pad_trim;

    /* Set the configuration */
    /* Set mode to Device mode */
    USB->HOST.CTRLA.bit.MODE = 0;
    /* Enable Run in Standby */
    USB->HOST.CTRLA.bit.RUNSTDBY = true;
    /* Set the descriptor address */
    USB->HOST.DESCADD.reg = (uint32_t)(&usb_endpoint_table[0]);
    /* Set speed configuration to Full speed */
    USB->DEVICE.CTRLB.bit.SPDCONF = USB_DEVICE_CTRLB_SPDCONF_FS_Val;
    /* Attach to the USB host */
    USB->DEVICE.CTRLB.reg &= ~USB_DEVICE_CTRLB_DETACH;

    /* Initialize endpoint table RAM location to a known value 0 */
    memset((uint8_t *)(&usb_endpoint_table[0]), 0, sizeof(usb_endpoint_table));
}

//*----------------------------------------------------------------------------
//* \fn    USB_IsConfigured
//* \brief Test if the device is configured and handle
// enumerationDEVICE.DeviceEndpoint[ep_num].EPCFG.bit.EPTYPE1
//*----------------------------------------------------------------------------
bool USB_Ok() {
    timerTick();

    /* Check for End of Reset flag */
    if (USB->DEVICE.INTFLAG.reg & USB_DEVICE_INTFLAG_EORST) {
        /* Clear the flag */
        USB->DEVICE.INTFLAG.reg = USB_DEVICE_INTFLAG_EORST;
        /* Set Device address as 0 */
        USB->DEVICE.DADD.reg = USB_DEVICE_DADD_ADDEN | 0;
        /* Configure endpoint 0 */
        /* Configure Endpoint 0 for Control IN and Control OUT */
        USB->DEVICE.DeviceEndpoint[0].EPCFG.reg =
            USB_DEVICE_EPCFG_EPTYPE0(1) | USB_DEVICE_EPCFG_EPTYPE1(1);
        USB->DEVICE.DeviceEndpoint[0].EPSTATUSSET.reg = USB_DEVICE_EPSTATUSSET_BK0RDY;
        USB->DEVICE.DeviceEndpoint[0].EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_BK1RDY;
        /* Configure control OUT Packet size to 64 bytes */
        usb_endpoint_table[0].DeviceDescBank[0].PCKSIZE.bit.SIZE = 3;
        /* Configure control IN Packet size to 64 bytes */
        usb_endpoint_table[0].DeviceDescBank[1].PCKSIZE.bit.SIZE = 3;
        /* Configure the data buffer address for control OUT */
        usb_endpoint_table[0].DeviceDescBank[0].ADDR.reg = (uint32_t)&ctrlOutCache.buf;
        /* Configure the data buffer address for control IN */
        usb_endpoint_table[0].DeviceDescBank[1].ADDR.reg = (uint32_t)&endpointCache[0].buf;
        /* Set Multipacket size to 8 for control OUT and byte count to 0*/
        usb_endpoint_table[0].DeviceDescBank[0].PCKSIZE.bit.MULTI_PACKET_SIZE = 8;
        usb_endpoint_table[0].DeviceDescBank[0].PCKSIZE.bit.BYTE_COUNT = 0;
        USB->DEVICE.DeviceEndpoint[0].EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_BK0RDY;

        // Reset current configuration value to 0
        currentConfiguration = 0;
    } else if (USB->DEVICE.DeviceEndpoint[0].EPINTFLAG.reg & USB_DEVICE_EPINTFLAG_RXSTP) {
        AT91F_CDC_Enumerate();
    }

    return currentConfiguration != 0;
}

//*----------------------------------------------------------------------------
//* \fn    USB_Read
//* \brief Read available data from Endpoint OUT
//*----------------------------------------------------------------------------
uint32_t USB_ReadCore(void *pData, uint32_t length, uint32_t ep, PacketBuffer *cache) {
    uint32_t packetSize = 0;
    UsbDeviceDescriptor *epdesc = (UsbDeviceDescriptor *)USB->HOST.DESCADD.reg + ep;

    if (!cache) {
        cache = &endpointCache[ep];
#if USE_HID || USE_WEBUSB
        assert(ep != USB_EP_HID && ep != USB_EP_WEB);
#endif
        timerTick();
    }

    if (cache->ptr < cache->size) {
        packetSize = MIN(cache->size - cache->ptr, length);
        if (pData) {
            memcpy(pData, cache->buf + cache->ptr, packetSize);
            cache->ptr += packetSize;
        }
        return packetSize;
    }

    if (!cache->read_job) {
        /* Set the buffer address for ep data */
        epdesc->DeviceDescBank[0].ADDR.reg = (uint32_t)&cache->buf;
        /* Set the byte count as zero */
        epdesc->DeviceDescBank[0].PCKSIZE.bit.BYTE_COUNT = 0;
        /* Set the byte count as zero */
        epdesc->DeviceDescBank[0].PCKSIZE.bit.MULTI_PACKET_SIZE = 0;
        /* Start the reception by clearing the bank 0 ready bit */
        USB->DEVICE.DeviceEndpoint[ep].EPSTATUSCLR.bit.BK0RDY = true;
        /* set the user flag */
        cache->read_job = true;
    }

    /* Check for Transfer Complete 0 flag */
    if (USB->DEVICE.DeviceEndpoint[ep].EPINTFLAG.reg & USB_DEVICE_EPINTFLAG_TRCPT0) {
        /* Set packet size */
        cache->size = epdesc->DeviceDescBank[0].PCKSIZE.bit.BYTE_COUNT;

        // this is when processing a hand-over
        if (cache->read_job == 2) {
            memcpy(&cache->buf, (void *)epdesc->DeviceDescBank[0].ADDR.reg, cache->size);
        }

        packetSize = MIN(cache->size, length);
        if (pData) {
            cache->ptr = packetSize;
            /* Copy read data to user buffer */
            memcpy(pData, cache->buf, packetSize);
        } else {
            cache->ptr = 0;
        }
        /* Clear the Transfer Complete 0 flag */
        USB->DEVICE.DeviceEndpoint[ep].EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_TRCPT0;
        /* Clear the user flag */
        cache->read_job = false;
    }

    // if (packetSize)
    //    logval("read", packetSize);

    return packetSize;
}

uint32_t USB_Read(void *pData, uint32_t length, uint32_t ep) {
    return USB_ReadCore(pData, length, ep, 0);
}

void USB_ReadBlocking(void *dst, uint32_t length, uint32_t ep, PacketBuffer *cache) {
    /* Blocking read till specified number of bytes is received */
    while (length) {
        uint32_t curr = USB_ReadCore(dst, length, ep, cache);
        // if (curr > 0)
        //    logval("readbl", length);
        length -= curr;
        dst = (char *)dst + curr;
    }
}

uint32_t USB_Write(const void *pData, uint32_t length, uint8_t ep_num) {
    return USB_WriteCore(pData, length, ep_num, false);
}

uint32_t USB_WriteCore(const void *pData, uint32_t length, uint8_t ep_num, bool handoverMode) {
    uint32_t data_address;

    UsbDeviceDescriptor *epdesc = (UsbDeviceDescriptor *)USB->HOST.DESCADD.reg + ep_num;

    if (handoverMode) {
        data_address = (uint32_t)pData;
        epdesc->DeviceDescBank[1].PCKSIZE.bit.AUTO_ZLP = false;
    }
    /* Check for requirement for multi-packet or auto zlp */
    else if (length >= (1 << (epdesc->DeviceDescBank[1].PCKSIZE.bit.SIZE + 3))) {
        /* Update the EP data address */
        data_address = (uint32_t)pData;
        // data must be in RAM!
        assert(data_address >= HMCRAMC0_ADDR);

        // always disable AUTO_ZLP on MSC channel, otherwise enable
        epdesc->DeviceDescBank[1].PCKSIZE.bit.AUTO_ZLP = ep_num == USB_EP_MSC_IN ? false : true;
    } else {
        /* Copy to local buffer */
        memcpy(endpointCache[ep_num].buf, pData, length);
        /* Update the EP data address */
        data_address = (uint32_t)&endpointCache[ep_num].buf;
    }

    /* Set the buffer address for ep data */
    epdesc->DeviceDescBank[1].ADDR.reg = data_address;
    /* Set the byte count as zero */
    epdesc->DeviceDescBank[1].PCKSIZE.bit.BYTE_COUNT = length;
    /* Set the multi packet size as zero for multi-packet transfers where length
     * > ep size */
    epdesc->DeviceDescBank[1].PCKSIZE.bit.MULTI_PACKET_SIZE = 0;
    /* Clear the transfer complete flag  */
    USB->DEVICE.DeviceEndpoint[ep_num].EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_TRCPT1;
    /* Set the bank as ready */
    USB->DEVICE.DeviceEndpoint[ep_num].EPSTATUSSET.reg = USB_DEVICE_EPSTATUSSET_BK1RDY;

    /* Wait for transfer to complete */
    while (!(USB->DEVICE.DeviceEndpoint[ep_num].EPINTFLAG.reg & USB_DEVICE_EPINTFLAG_TRCPT1)) {
        // if (ep_num && !USB_Ok())
        //    return -1;
    }

    return length;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_USB_SendZlp
//* \brief Send zero length packet through the control endpoint
//*----------------------------------------------------------------------------
void AT91F_USB_SendZlp(void) {
    uint8_t c;
    USB_Write(&c, 0, 0);
}

static void configureInOut(uint8_t in_ep) {
    /* Configure BULK OUT endpoint for CDC Data interface*/
    USB->DEVICE.DeviceEndpoint[in_ep + 1].EPCFG.reg = USB_DEVICE_EPCFG_EPTYPE0(3);
    /* Set maximum packet size as 64 bytes */
    usb_endpoint_table[in_ep + 1].DeviceDescBank[0].PCKSIZE.bit.SIZE = 3;
    USB->DEVICE.DeviceEndpoint[in_ep + 1].EPSTATUSSET.reg = USB_DEVICE_EPSTATUSSET_BK0RDY;
    /* Configure the data buffer */
    usb_endpoint_table[in_ep + 1].DeviceDescBank[0].ADDR.reg =
        (uint32_t)&endpointCache[in_ep + 1].buf;

    /* Configure BULK IN endpoint for CDC Data interface */
    USB->DEVICE.DeviceEndpoint[in_ep].EPCFG.reg = USB_DEVICE_EPCFG_EPTYPE1(3);
    /* Set maximum packet size as 64 bytes */
    usb_endpoint_table[in_ep].DeviceDescBank[1].PCKSIZE.bit.SIZE = 3;
    USB->DEVICE.DeviceEndpoint[in_ep].EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_BK1RDY;
    /* Configure the data buffer */
    usb_endpoint_table[in_ep].DeviceDescBank[1].ADDR.reg = (uint32_t)&endpointCache[in_ep].buf;
}

static uint16_t wLength;

static void sendCtrl(const void *data, uint32_t len) { USB_Write(data, MIN(len, wLength), 0); }

//*----------------------------------------------------------------------------
//* \fn    AT91F_CDC_Enumerate
//* \brief This function is a callback invoked when a SETUP packet is received
//*----------------------------------------------------------------------------
void AT91F_CDC_Enumerate() {
    uint8_t bmRequestType, bRequest, dir;
    uint16_t wValue, wIndex, wStatus;

    /* Clear the Received Setup flag */
    USB->DEVICE.DeviceEndpoint[0].EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_RXSTP;

    /* Read the USB request parameters */
    bmRequestType = ctrlOutCache.buf[0];
    bRequest = ctrlOutCache.buf[1];
    wValue = (ctrlOutCache.buf[2] & 0xFF);
    wValue |= (ctrlOutCache.buf[3] << 8);
    wIndex = (ctrlOutCache.buf[4] & 0xFF);
    wIndex |= (ctrlOutCache.buf[5] << 8);
    wLength = (ctrlOutCache.buf[6] & 0xFF);
    wLength |= (ctrlOutCache.buf[7] << 8);

    /* Clear the Bank 0 ready flag on Control OUT */
    USB->DEVICE.DeviceEndpoint[0].EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_BK0RDY;

    uint32_t reqId = (bRequest << 8) | bmRequestType;

    logwrite("USBReq: ");
    logwritenum(reqId);
    logwrite(" wValue: ");
    logwritenum(wValue);
    logwrite(" wIndex: ");
    logwritenum(wIndex);
    logval(" wLen", wLength);

    /* Handle supported standard device request Cf Table 9-3 in USB
     * specification Rev 1.1 */
    switch (reqId) {
    case STD_GET_DESCRIPTOR1:
    case STD_GET_DESCRIPTOR:
        if (wValue == 0x100)
            /* Return Device Descriptor */
            sendCtrl(devDescriptor, sizeof(devDescriptor));
        else if (wValue == 0x200)
            /* Return Configuration Descriptor */
            sendCtrl(cfgDescriptor, sizeof(cfgDescriptor));
        else if (ctrlOutCache.buf[3] == 3) {
            if (ctrlOutCache.buf[2] >= STRING_DESCRIPTOR_COUNT)
            {
                stall_ep(0);
            } else {
                StringDescriptor desc = {0};
                desc.type = 3;
                if (ctrlOutCache.buf[2] == 0) {
                    desc.len = 4;
                    desc.data[0] = 0x09;
                    desc.data[1] = 0x04;
                } else {
                load_serial_number(serial_number);
                    const char *ptr = string_descriptors[ctrlOutCache.buf[2]];
                    desc.len = strlen(ptr) * 2 + 2;
                    for (int i = 0; ptr[i]; i++) {
                        desc.data[i * 2] = ptr[i];
                    }
                }
                sendCtrl(&desc, desc.len);
            }
        } else if (ctrlOutCache.buf[3] == 0x0F) {
            sendCtrl(bosDescriptor, sizeof(bosDescriptor));
        }
#if USE_HID
        else if (ctrlOutCache.buf[3] == 0x21) {
            sendCtrl(hidCfgDescriptor, sizeof(hidCfgDescriptor));
        } else if (ctrlOutCache.buf[3] == 0x22) {
            sendCtrl(hidDescriptor, sizeof(hidDescriptor));
        }
#endif
        else {
            /* Stall the request */
            stall_ep(0);
        }
        break;
#if USE_WEBUSB
    case STD_VENDOR_CTRL1:
        stall_ep(0);
        break;
    case STD_VENDOR_CTRL2:
        if (wIndex == 0x07)
            sendCtrl(msOS20Descriptor, sizeof(msOS20Descriptor));
        else
            stall_ep(0);
        break;
#endif
    case STD_SET_ADDRESS:
        /* Send ZLP */
        AT91F_USB_SendZlp();
        /* Set device address to the newly received address from host */
        USB->DEVICE.DADD.reg = USB_DEVICE_DADD_ADDEN | wValue;
        break;
    case STD_SET_CONFIGURATION:
        /* Store configuration */
        currentConfiguration = (uint8_t)wValue;
        /* Send ZLP */
        AT91F_USB_SendZlp();

#if USE_CDC
        configureInOut(USB_EP_IN);

        /* Configure INTERRUPT IN endpoint for CDC COMM interface*/
        USB->DEVICE.DeviceEndpoint[USB_EP_COMM].EPCFG.reg = USB_DEVICE_EPCFG_EPTYPE1(4);
        /* Set maximum packet size as 64 bytes */
        usb_endpoint_table[USB_EP_COMM].DeviceDescBank[1].PCKSIZE.bit.SIZE = 0;
        USB->DEVICE.DeviceEndpoint[USB_EP_COMM].EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_BK1RDY;
#endif

        configureInOut(USB_EP_MSC_IN);

#if USE_HID
        /* Configure INTERRUPT IN/OUT endpoint for HID interface*/
        USB->DEVICE.DeviceEndpoint[USB_EP_HID].EPCFG.reg =
            USB_DEVICE_EPCFG_EPTYPE0(4) | USB_DEVICE_EPCFG_EPTYPE1(4);

        USB->DEVICE.DeviceEndpoint[USB_EP_HID].EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSSET_BK0RDY;
        USB->DEVICE.DeviceEndpoint[USB_EP_HID].EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_BK1RDY;
        usb_endpoint_table[USB_EP_HID].DeviceDescBank[0].PCKSIZE.bit.SIZE = 3;
        usb_endpoint_table[USB_EP_HID].DeviceDescBank[1].PCKSIZE.bit.SIZE = 3;
#endif

#if USE_WEBUSB
        /* Configure INTERRUPT IN/OUT endpoint for HID interface*/
        USB->DEVICE.DeviceEndpoint[USB_EP_WEB].EPCFG.reg =
            USB_DEVICE_EPCFG_EPTYPE0(4) | USB_DEVICE_EPCFG_EPTYPE1(4);

        USB->DEVICE.DeviceEndpoint[USB_EP_WEB].EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSSET_BK0RDY;
        USB->DEVICE.DeviceEndpoint[USB_EP_WEB].EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_BK1RDY;
        usb_endpoint_table[USB_EP_WEB].DeviceDescBank[0].PCKSIZE.bit.SIZE = 3;
        usb_endpoint_table[USB_EP_WEB].DeviceDescBank[1].PCKSIZE.bit.SIZE = 3;
#endif

        break;

    case STD_GET_CONFIGURATION:
        /* Return current configuration value */
        sendCtrl(&(currentConfiguration), sizeof(currentConfiguration));
        break;

    case STD_GET_STATUS_ZERO:
        wStatus = 0;
        sendCtrl(&wStatus, sizeof(wStatus));
        break;
    case STD_GET_STATUS_INTERFACE:
        wStatus = 0;
        sendCtrl(&wStatus, sizeof(wStatus));
        break;
    case STD_GET_STATUS_ENDPOINT:
        wStatus = 0;
        dir = wIndex & 80;
        wIndex &= 0x0F;
        if (wIndex < MAX_EP) {
            if (dir) {
                wStatus = (USB->DEVICE.DeviceEndpoint[wIndex].EPSTATUS.reg &
                           USB_DEVICE_EPSTATUSSET_STALLRQ1)
                              ? 1
                              : 0;
            } else {
                wStatus = (USB->DEVICE.DeviceEndpoint[wIndex].EPSTATUS.reg &
                           USB_DEVICE_EPSTATUSSET_STALLRQ0)
                              ? 1
                              : 0;
            }
            /* Return current status of endpoint */
            sendCtrl(&wStatus, sizeof(wStatus));
        } else
            /* Stall the request */
            stall_ep(0);
        break;
    case STD_SET_FEATURE_ZERO:
        /* Stall the request */
        stall_ep(0);
        break;
    case STD_SET_FEATURE_INTERFACE:
        /* Send ZLP */
        AT91F_USB_SendZlp();
        break;
    case STD_SET_FEATURE_ENDPOINT:
        dir = wIndex & 0x80;
        wIndex &= 0x0F;
        if ((wValue == 0) && wIndex && (wIndex < MAX_EP)) {
            /* Set STALL request for the endpoint */
            if (dir) {
                USB->DEVICE.DeviceEndpoint[wIndex].EPSTATUSSET.reg =
                    USB_DEVICE_EPSTATUSSET_STALLRQ1;
            } else {
                USB->DEVICE.DeviceEndpoint[wIndex].EPSTATUSSET.reg =
                    USB_DEVICE_EPSTATUSSET_STALLRQ0;
            }
            /* Send ZLP */
            AT91F_USB_SendZlp();
        } else
            /* Stall the request */
            stall_ep(0);
        break;
    case STD_CLEAR_FEATURE_ZERO:
        /* Stall the request */
        stall_ep(0);
        break;
    case STD_CLEAR_FEATURE_INTERFACE:
        /* Send ZLP */
        AT91F_USB_SendZlp();
        break;
    case STD_CLEAR_FEATURE_ENDPOINT:
        dir = wIndex & 0x80;
        wIndex &= 0x0F;
        if ((wValue == 0) && wIndex && (wIndex < MAX_EP)) {
            if (dir) {
                if (USB->DEVICE.DeviceEndpoint[wIndex].EPSTATUS.reg &
                    USB_DEVICE_EPSTATUSSET_STALLRQ1) {
                    // Remove stall request
                    USB->DEVICE.DeviceEndpoint[wIndex].EPSTATUSCLR.reg =
                        USB_DEVICE_EPSTATUSCLR_STALLRQ1;
                    if (USB->DEVICE.DeviceEndpoint[wIndex].EPINTFLAG.reg &
                        USB_DEVICE_EPINTFLAG_STALL1) {
                        USB->DEVICE.DeviceEndpoint[wIndex].EPINTFLAG.reg =
                            USB_DEVICE_EPINTFLAG_STALL1;
                        // The Stall has occurred, then reset data toggle
                        USB->DEVICE.DeviceEndpoint[wIndex].EPSTATUSCLR.reg =
                            USB_DEVICE_EPSTATUSSET_DTGLIN;
                    }
                }
            } else {
                if (USB->DEVICE.DeviceEndpoint[wIndex].EPSTATUS.reg &
                    USB_DEVICE_EPSTATUSSET_STALLRQ0) {
                    // Remove stall request
                    USB->DEVICE.DeviceEndpoint[wIndex].EPSTATUSCLR.reg =
                        USB_DEVICE_EPSTATUSCLR_STALLRQ0;
                    if (USB->DEVICE.DeviceEndpoint[wIndex].EPINTFLAG.reg &
                        USB_DEVICE_EPINTFLAG_STALL0) {
                        USB->DEVICE.DeviceEndpoint[wIndex].EPINTFLAG.reg =
                            USB_DEVICE_EPINTFLAG_STALL0;
                        // The Stall has occurred, then reset data toggle
                        USB->DEVICE.DeviceEndpoint[wIndex].EPSTATUSCLR.reg =
                            USB_DEVICE_EPSTATUSSET_DTGLOUT;
                    }
                }
            }
            /* Send ZLP */
            AT91F_USB_SendZlp();
        } else {
            stall_ep(0);
        }
        break;

#if USE_CDC
    // handle CDC class requests
    case SET_LINE_CODING:
        /* Send ZLP */
        AT91F_USB_SendZlp();
        break;
    case GET_LINE_CODING:
        /* Send current line coding */
        sendCtrl(&line_coding, sizeof(usb_cdc_line_coding_t));
        break;
    case SET_CONTROL_LINE_STATE:
        /* Store the current connection */
        pCdc.currentConnection = wValue;
        /* Send ZLP */
        AT91F_USB_SendZlp();
        break;
#endif

#if USE_MSC_CHECKS
    // MSC
    case MSC_RESET:
        DBG_MSC(logmsg("MSC reset"));
        msc_reset();
        break;
#endif

    case MSC_GET_MAX_LUN:
        DBG_MSC(logmsg("MSC maxlun"));
        wStatus = MAX_LUN;
        sendCtrl(&wStatus, 1);
        break;

#if USE_HID
    case HID_REQUEST_GET_PROTOCOL:
    case HID_REQUEST_GET_IDLE:
    case HID_REQUEST_GET_REPORT: {
        uint8_t buf[8] = {0};
        sendCtrl(buf, 8);
    } break;

    case HID_REQUEST_SET_IDLE:
    case HID_REQUEST_SET_REPORT:
    case HID_REQUEST_SET_PROTOCOL:
        AT91F_USB_SendZlp();
        break;
#endif

    default:
        logval("Invalid CTRL command", reqId);
        /* Stall the request */
        stall_ep(0);
        break;
    }
}

static bool isInEP(uint8_t ep) { return ep == USB_EP_IN || ep == USB_EP_MSC_IN; }

void reset_ep(uint8_t ep) {
    assert(ep != 0);

    // Stop transfer
    if (isInEP(ep)) {
        USB->DEVICE.DeviceEndpoint[ep].EPSTATUSCLR.reg = USB_DEVICE_EPSTATUSCLR_BK1RDY;
        // Eventually ack a transfer occur during abort
        USB->DEVICE.DeviceEndpoint[ep].EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_TRCPT1;
    } else {
        USB->DEVICE.DeviceEndpoint[ep].EPSTATUSSET.reg = USB_DEVICE_EPSTATUSSET_BK0RDY;
        // Eventually ack a transfer occur during abort
        USB->DEVICE.DeviceEndpoint[ep].EPINTFLAG.reg = USB_DEVICE_EPINTFLAG_TRCPT0;
    }
}

void stall_ep(uint8_t ep) {
    logval("Stall EP", ep);
    /* Check the direction */
    if (ep == 0 || isInEP(ep)) {
        /* Set STALL request on IN direction */
        USB->DEVICE.DeviceEndpoint[ep].EPSTATUSSET.reg = USB_DEVICE_EPSTATUSSET_STALLRQ1;
    } else {
        /* Set STALL request on OUT direction */
        USB->DEVICE.DeviceEndpoint[ep].EPSTATUSSET.reg = USB_DEVICE_EPSTATUSSET_STALLRQ0;
    }
}

void usb_init(void) {
    /* Initialize USB */
    AT91F_InitUSB();
    currentConfiguration = 0;
#if USE_CDC
    pCdc.currentConnection = 0;
#endif
    USB->HOST.CTRLA.bit.ENABLE = true;
}

#include "usart_sam_ba.h"

#if USE_UART
#define UART(e)                                                                                    \
    if (b_sam_ba_interface_usart)                                                                  \
        return e;
#else
#define UART(e)
#endif

bool cdc_is_rx_ready(void) {
    UART(usart_is_rx_ready())

    /* Check whether the device is configured */
    if (!USB_Ok())
        return 0;

    /* Return transfer complete 0 flag status */
    return (USB->DEVICE.DeviceEndpoint[USB_EP_OUT].EPINTFLAG.reg & USB_DEVICE_EPINTFLAG_TRCPT0);
}

uint32_t cdc_write_buf(void const *data, uint32_t length) {
    UART(usart_putdata(data, length))
    /* Send the specified number of bytes on USB CDC */
    USB_Write((const char *)data, length, USB_EP_IN);
    return length;
}

uint32_t cdc_write_buf_xmd(void const *data, uint32_t length) {
    UART(usart_putdata_xmd(data, length))
    /* Send the specified number of bytes on USB CDC */
    USB_Write((const char *)data, length, USB_EP_IN);
    return length;
}

uint32_t cdc_read_buf(void *data, uint32_t length) {
    UART(usart_getdata(data, length))

    /* Check whether the device is configured */
    if (!USB_Ok())
        return 0;

    /* Read from USB CDC */
    return USB_Read((char *)data, length, USB_EP_OUT);
}

uint32_t cdc_read_buf_xmd(void *data, uint32_t length) {
    UART(usart_getdata_xmd(data, length))
    /* Check whether the device is configured */
    if (!USB_Ok())
        return 0;

    /* Blocking read till specified number of bytes is received */
    USB_ReadBlocking((char *)data, length, USB_EP_OUT, 0);

    return length;
}
