#include "XboxControllerPico.h"
#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE 1
#endif
#include "tusb.h"
#include "device/usbd_pvt.h"

// 送信データの実体
ReportDataXinput_t XboxButtonData;

// エンドポイントアドレス
uint8_t endpoint_in = 0;
uint8_t endpoint_out = 0;

// Device Descriptor
tusb_desc_device_t const xinputDeviceDescriptor = {
  .bLength = sizeof(tusb_desc_device_t),
  .bDescriptorType = TUSB_DESC_DEVICE,
  .bcdUSB = 0x0200,
  .bDeviceClass = 0xFF,
  .bDeviceSubClass = 0xFF,
  .bDeviceProtocol = 0xFF,
  .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

  .idVendor = 0x045E,  // Microsoft
  .idProduct = 0x028E, // Xbox 360 Controller
  .bcdDevice = 0x0572,

  .iManufacturer = 0x01,
  .iProduct = 0x02,
  .iSerialNumber = 0x03,

  .bNumConfigurations = 0x01
};

// Device Descriptor Callback
uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)&xinputDeviceDescriptor;
}

// Configuration Descriptor (XInput独自構造)
const uint8_t xinputConfigurationDescriptor[] = {
  // Configuration Descriptor:
  0x09, // bLength
  0x02, // bDescriptorType
  0x30, 0x00, // wTotalLength (48 bytes)
  0x01, // bNumInterfaces
  0x01, // bConfigurationValue
  0x00, // iConfiguration
  0x80, // bmAttributes (Bus-powered)
  0xFA, // bMaxPower (500 mA)

  // Interface Descriptor:
  0x09, // bLength
  0x04, // bDescriptorType
  0x00, // bInterfaceNumber
  0x00, // bAlternateSetting
  0x02, // bNumEndPoints
  0xFF, // bInterfaceClass (Vendor specific)
  0x5D, // bInterfaceSubClass
  0x01, // bInterfaceProtocol
  0x00, // iInterface

  // Microsoft XInput Unknown Descriptor (XInput動作に必要)
  0x10, 0x21, 0x10, 0x01, 0x01, 0x24, 0x81, 0x14, 0x03, 0x00, 0x03, 0x13, 0x02, 0x00, 0x03, 0x00,

  // Endpoint Descriptor (IN 1):
  0x07, // bLength
  0x05, // bDescriptorType
  0x81, // bEndpointAddress (IN 1)
  0x03, // bmAttributes (Interrupt)
  0x20, 0x00, // wMaxPacketSize (32 bytes)
  0x04, // bInterval (4 frames = 4ms)

  // Endpoint Descriptor (OUT 2):
  0x07, // bLength
  0x05, // bDescriptorType
  0x02, // bEndpointAddress (OUT 2)
  0x03, // bmAttributes (Interrupt)
  0x20, 0x00, // wMaxPacketSize (32 bytes)
  0x08, // bInterval (8 frames = 8ms)
};

// Configuration Descriptor Callback
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return xinputConfigurationDescriptor;
}

// String Descriptor
char const *string_desc_arr_xinput[] = {
  (const char[]){0x09, 0x04}, // 0: English (0x0409)
  "Microsoft",                // 1: Manufacturer
  "Xbox 360 Controller",      // 2: Product
  "1.0"                       // 3: Serials
};

static uint16_t _desc_str[32];

// String Descriptor Callback
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;
  uint8_t chr_count;

  if (index == 0) {
    memcpy(&_desc_str[1], string_desc_arr_xinput[0], 2);
    chr_count = 1;
  } else {
    if (!(index < sizeof(string_desc_arr_xinput) / sizeof(string_desc_arr_xinput[0]))) {
      return NULL;
    }
    const char *str = string_desc_arr_xinput[index];
    chr_count = strlen(str);
    if (chr_count > 31) chr_count = 31;

    for (uint8_t i = 0; i < chr_count; i++) {
      _desc_str[1 + i] = str[i];
    }
  }

  _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
  return _desc_str;
}

// HID Report Descriptor Callback
uint8_t const *tud_hid_descriptor_report_cb(void) {
  return NULL;
}

static void xinput_init(void) {}
static void xinput_reset(uint8_t __unused rhport) {}

static uint16_t xinput_open(uint8_t __unused rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len) {
  uint16_t const drv_len = sizeof(tusb_desc_interface_t) + itf_desc->bNumEndpoints * sizeof(tusb_desc_endpoint_t) + 16;
  TU_VERIFY(max_len >= drv_len, 0);

  uint8_t const *p_desc = tu_desc_next(itf_desc);
  uint8_t found_endpoints = 0;
  while ((found_endpoints < itf_desc->bNumEndpoints) && (drv_len <= max_len)) {
    tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *)p_desc;
    if (TUSB_DESC_ENDPOINT == tu_desc_type(desc_ep)) {
      TU_ASSERT(usbd_edpt_open(rhport, desc_ep));

      if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
        endpoint_in = desc_ep->bEndpointAddress;
      } else {
        endpoint_out = desc_ep->bEndpointAddress;
      }
      found_endpoints += 1;
    }
    p_desc = tu_desc_next(p_desc);
  }
  return drv_len;
}

static bool xinput_device_control_request(uint8_t __unused rhport, uint8_t stage, tusb_control_request_t const *request) {
  (void) stage;
  (void) request;
  return true;
}

static bool xinput_xfer_cb(uint8_t __unused rhport, uint8_t __unused ep_addr, xfer_result_t __unused result, uint32_t __unused xferred_bytes) {
  return true;
}

static usbd_class_driver_t const xinput_driver = {
#if CFG_TUSB_DEBUG >= 2
  .name = "XINPUT",
#endif
  .init             = xinput_init,
  .deinit           = NULL,
  .reset            = xinput_reset,
  .open             = xinput_open,
  .control_xfer_cb  = xinput_device_control_request,
  .xfer_cb          = xinput_xfer_cb,
  .sof              = NULL
};

extern "C" usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count) {
  *driver_count = 1;
  return &xinput_driver;
}

void xboxcontroller_init(void) {
  memset(&XboxButtonData, 0, sizeof(ReportDataXinput_t));
  XboxButtonData.message_type = 0x00;
  XboxButtonData.packet_size = 0x14;
  tusb_init();
}

void xboxcontroller_reset(void) {
  memset(&XboxButtonData, 0, sizeof(ReportDataXinput_t));
  XboxButtonData.message_type = 0x00;
  XboxButtonData.packet_size = 0x14;
  XboxButtonData.l_x = 0;
  XboxButtonData.l_y = 0;
  XboxButtonData.r_x = 0;
  XboxButtonData.r_y = 0;
}

bool xboxcontroller_send_report(void) {
  if (tud_suspended()) {
    tud_remote_wakeup();
  }

  if (tud_mounted() && (endpoint_in != 0) && !usbd_edpt_busy(0, endpoint_in)) {
    usbd_edpt_claim(0, endpoint_in);
    usbd_edpt_xfer(0, endpoint_in, (uint8_t *)&XboxButtonData, sizeof(ReportDataXinput_t));
    usbd_edpt_release(0, endpoint_in);
    return true;
  }
  return false;
}
