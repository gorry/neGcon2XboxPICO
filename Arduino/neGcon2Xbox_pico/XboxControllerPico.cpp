#undef CFG_TUD_MSC
#define CFG_TUD_MSC 1
#include "XboxControllerPico.h"
#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE 1
#endif
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include <Adafruit_NeoPixel.h>
#include <FatFS.h>

// 外部フラッシュ領域 (__FS_START, __FS_END) を参照するための extern 宣言
extern uint8_t _FS_start;
extern uint8_t _FS_end;
extern Adafruit_NeoPixel pixels;

#define MSC_BLOCK_SIZE 512

// MSC用のバッファセクタ
static uint8_t flash_sector_buf[FLASH_SECTOR_SIZE] __attribute__((aligned(4)));

// グローバルなMSCモードフラグ
bool usb_mode_msc = false;
bool config_file_writing = false;

extern volatile unsigned long last_msc_access_time;
extern volatile uint8_t msc_access_type;
extern volatile bool msc_active_connected;

static uint32_t last_msc_write_time = 0;
static bool msc_write_dirty = false;

// MSC 用の Device Descriptor
tusb_desc_device_t const mscDeviceDescriptor = {
  .bLength = sizeof(tusb_desc_device_t),
  .bDescriptorType = TUSB_DESC_DEVICE,
  .bcdUSB = 0x0200, // USB 2.00 に戻す
  .bDeviceClass = 0x00,
  .bDeviceSubClass = 0x00,
  .bDeviceProtocol = 0x00,
  .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

  .idVendor = 0xCAFE,  // ダミーのVID
  .idProduct = 0x4005, // キャッシュリセットのため 0x4004 -> 0x4005 に変更
  .bcdDevice = 0x0100,

  .iManufacturer = 0x01,
  .iProduct = 0x02,
  .iSerialNumber = 0x03,

  .bNumConfigurations = 0x01
};

// MSC 用 of Configuration Descriptor (合計 32 バイト)
const uint8_t mscConfigurationDescriptor[] = {
  // Configuration Descriptor:
  0x09, // bLength
  0x02, // bDescriptorType
  0x20, 0x00, // wTotalLength (32 bytes)
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
  0x08, // bInterfaceClass (Mass Storage)
  0x06, // bInterfaceSubClass (SCSI transparent command set)
  0x50, // bInterfaceProtocol (Bulk-Only Transport)
  0x00, // iInterface

  // Endpoint Descriptor (Bulk IN 1):
  0x07, // bLength
  0x05, // bDescriptorType
  0x81, // bEndpointAddress (IN 1)
  0x02, // bmAttributes (Bulk)
  0x40, 0x00, // wMaxPacketSize (64 bytes)
  0x00, // bInterval

  // Endpoint Descriptor (Bulk OUT 2):
  0x07, // bLength
  0x05, // bDescriptorType
  0x02, // bEndpointAddress (OUT 2)
  0x02, // bmAttributes (Bulk)
  0x40, 0x00, // wMaxPacketSize (64 bytes)
  0x00, // bInterval
};

// MSC 用の String Descriptor
char const *string_desc_arr_msc[] = {
  (const char[]){0x09, 0x04}, // 0: English
  "Antigravity",              // 1: Manufacturer
  "neGcon Config Disk",       // 2: Product
  "1234567C"                  // キャッシュリセットのためシリアル番号を変更
};

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
  if (usb_mode_msc) {
    return (uint8_t const *)&mscDeviceDescriptor;
  } else {
    return (uint8_t const *)&xinputDeviceDescriptor;
  }
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
  if (usb_mode_msc) {
    return mscConfigurationDescriptor;
  } else {
    return xinputConfigurationDescriptor;
  }
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
    if (usb_mode_msc) {
      memcpy(&_desc_str[1], string_desc_arr_msc[0], 2);
    } else {
      memcpy(&_desc_str[1], string_desc_arr_xinput[0], 2);
    }
    chr_count = 1;
  } else {
    char const **str_arr = usb_mode_msc ? string_desc_arr_msc : string_desc_arr_xinput;
    uint8_t max_size = usb_mode_msc ? (sizeof(string_desc_arr_msc) / sizeof(string_desc_arr_msc[0])) : (sizeof(string_desc_arr_xinput) / sizeof(string_desc_arr_xinput[0]));

    if (!(index < max_size)) {
      return NULL;
    }
    const char *str = str_arr[index];
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

extern "C" {
  void mscd_init(void);
  bool mscd_deinit(void);
  void mscd_reset(uint8_t rhport);
  uint16_t mscd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
  bool mscd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
  bool mscd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);
}

static usbd_class_driver_t const msc_local_driver = {
#if CFG_TUSB_DEBUG >= 2
  .name = "MSC",
#endif
  .init             = mscd_init,
  .deinit           = mscd_deinit,
  .reset            = mscd_reset,
  .open             = mscd_open,
  .control_xfer_cb  = mscd_control_xfer_cb,
  .xfer_cb          = mscd_xfer_cb,
  .sof              = NULL
};

extern "C" usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count) {
  if (usb_mode_msc) {
    *driver_count = 1;
    return &msc_local_driver;
  } else {
    *driver_count = 1;
    return &xinput_driver;
  }
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

// =========================================================================
// USB MSC (Mass Storage Class) コールバック実装と再接続処理
// =========================================================================
extern "C" {

uint8_t tud_msc_get_maxlun_cb(void) {
  return 0; // LUNの最大インデックス（LUN数 - 1）。LUN数が1つの場合は0を返す。
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
  (void) lun; (void) power_condition; (void) start; (void) load_eject;
  return true;
}

bool tud_msc_prevent_allow_medium_removal_cb(uint8_t lun, uint8_t prohibit_removal, uint8_t control) {
  (void) lun; (void) prohibit_removal; (void) control;
  return true;
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
  (void) lun;
  const char vid[] = "Antigrav";
  const char pid[] = "neGcon Disk";
  const char rev[] = "1.0";

  memset(vendor_id, ' ', 8);
  memcpy(vendor_id, vid, tu_min32(strlen(vid), 8));

  memset(product_id, ' ', 16);
  memcpy(product_id, pid, tu_min32(strlen(pid), 16));

  memset(product_rev, ' ', 4);
  memcpy(product_rev, rev, tu_min32(strlen(rev), 4));
}

void tud_msc_read_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
  (void) lun;
  uint32_t start_addr = (uint32_t)&_FS_start;
  uint32_t end_addr = (uint32_t)&_FS_end;
  if (start_addr < 0x10000000) start_addr += 0x10000000;
  if (end_addr < 0x10000000) end_addr += 0x10000000;
  uint32_t flash_size = end_addr - start_addr;
  *block_size = MSC_BLOCK_SIZE;
  *block_count = flash_size / MSC_BLOCK_SIZE;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
  tud_msc_read_capacity_cb(lun, block_count, block_size);
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
  (void) lun; (void) scsi_cmd; (void) buffer; (void) bufsize;
  return -1;
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  (void) lun;
  return true;
}

bool tud_msc_is_write_protected_cb(uint8_t lun) {
  (void) lun;
  return config_file_writing;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  (void) lun;

  // 読み込みアクセスを通知 (青)
  last_msc_access_time = millis();
  msc_active_connected = true;
  if (offset == 0) {
    if (msc_access_type == 0) {
      msc_access_type = 1;
    }

#ifdef USE_LOG_MSC
    uint8_t* b = (uint8_t*)buffer;
    Serial.printf("[%lu] [MSC READ] lba:%u, bufsize:%u, offset:%u, buf_ptr:%p, before_data:0x%02X%02X%02X%02X\n", 
                  last_msc_access_time, lba, bufsize, offset, buffer, b[0], b[1], b[2], b[3]);
#endif


  }

  uint8_t temp_buf[MSC_BLOCK_SIZE];
  int res = fatfs::disk_read(0, temp_buf, lba, 1);
  if (res == fatfs::RES_OK) {
    memcpy(buffer, temp_buf + offset, bufsize);
    return bufsize;
  }
  Serial.printf("[%lu] [MSC READ FAILED] res:%d\n", millis(), res);
  return -1;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  (void) lun;

  // キャッシュへの書き込みタイミングでアクセスランプ(赤)を点灯
  // これは読み込みより優先するので、消灯判定は行わない
  last_msc_access_time = millis();
  msc_active_connected = true;
  if (offset == 0) {
    msc_access_type = 2;

#ifdef USE_LOG_MSC
    Serial.printf("[%lu] [MSC WRITE] lba:%u, bufsize:%u, offset:%u, buf_ptr:%p, data:0x%02X%02X%02X%02X\n", 
                  last_msc_access_time, lba, bufsize, offset, buffer, buffer[0], buffer[1], buffer[2], buffer[3]);
#endif

  }

  uint8_t temp_buf[MSC_BLOCK_SIZE];
  if (fatfs::disk_read(0, temp_buf, lba, 1) != fatfs::RES_OK) {
    memset(temp_buf, 0, MSC_BLOCK_SIZE);
  }
  memcpy(temp_buf + offset, buffer, bufsize);

  int res = fatfs::disk_write(0, temp_buf, lba, 1);
  if (res == fatfs::RES_OK) {
    last_msc_write_time = millis();
    msc_write_dirty = true;
    return bufsize;
  }
  Serial.printf("[%lu] [MSC WRITE FAILED] res:%d\n", millis(), res);
  return -1;
}

} // extern "C"

void tud_msc_sync_task(void) {
  if (msc_write_dirty && (millis() - last_msc_write_time >= 500)) {
    // 物理フラッシュへの書き出しタイミングでアクセスランプ(赤)を点灯
    // これは読み込みより優先するので、消灯判定は行わない
    last_msc_access_time = millis();
    msc_access_type = 2; // 書き込み：赤

    fatfs::disk_ioctl(0, CTRL_SYNC, nullptr);
    msc_write_dirty = false;
#ifdef USE_LOG_MSC
    Serial.printf("[%lu] [MSC SYNC] Cache flushed to physical flash.\n", millis());
#endif
  }
}

void xboxcontroller_reconnect(bool as_msc) {
  // BOOTSELボタンが離されるまで待つ
  // (リブート時にボタンが押されたままだとブートROMのRPI-RP2モードに入ってしまうのを防ぐため)
  while (BOOTSEL) {
    tud_task(); // USBスタックの処理を継続してフリーズを防ぐ
    delay(1);
  }
  delay(100); // 物理的なチャタリングと指が完全に離れるのを待つマージン

  // 再起動前に未フラッシュのデータを強制同期
  if (msc_write_dirty) {
    fatfs::disk_ioctl(0, CTRL_SYNC, nullptr);
    msc_write_dirty = false;
  }

  // 1. リセット開始を伝えるデバッグLED：赤色
  pixels.setPixelColor(0, pixels.Color(255, 0, 0));
  pixels.show();
  delay(100);

  if (as_msc) {
    watchdog_hw->scratch[0] = 0xAB0057C0;
  } else {
    watchdog_hw->scratch[0] = 0xAB00CAFE;
  }
  
  // Pico SDK標準の watchdog_reboot を使用して10ms後に再起動
  watchdog_reboot(0, 0, 10);
  while (1) {
    __asm("nop");
  }
}
