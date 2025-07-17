#include "system_utils.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <Arduino.h>

#ifdef CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED
   #include "hal/usb_serial_jtag_ll.h"
   #include "soc/usb_serial_jtag_reg.h"
   #include "soc/usb_serial_jtag_struct.h"
#endif

// Disable brownout detector
// This function is called before setup() using __attribute__((constructor))
void disableBrownout() {
    uint32_t val = READ_PERI_REG(RTC_CNTL_BROWN_OUT_REG);
    val &= ~(1 << 27);  // RTC_CNTL_BROWN_OUT_ENA
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, val);
}

// Disable USB Serial/JTAG interrupts to prevent spurious resets
void disableUsbJtagInterrupts() {
    #ifdef CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED
        usb_serial_jtag_ll_disable_intr_mask(0xFFFFFFFF);
        USB_SERIAL_JTAG.conf0.usb_pad_enable = 0;
    #endif
}

// Clear serial buffer from bootloader messages
void clearBootloaderSerialBuffer() {
    Serial.begin(115200);
    vTaskDelay(pdMS_TO_TICKS(100));
    while (Serial.available()) Serial.read();
    Serial.flush();
    Serial.end();
    vTaskDelay(pdMS_TO_TICKS(100));
}