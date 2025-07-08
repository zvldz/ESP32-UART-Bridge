#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// Device modes
typedef enum {
  MODE_NORMAL,
  MODE_FLASHING,
  MODE_CONFIG
} DeviceMode;

// LED modes
typedef enum {
  LED_MODE_OFF,
  LED_MODE_WIFI_ON,      // Constantly ON in WiFi config mode
  LED_MODE_DATA_FLASH    // Flash on data activity in normal mode
} LedMode;

// USB operation modes - MOVED HERE, BEFORE Config!
enum UsbMode {
    USB_MODE_DEVICE = 0,
    USB_MODE_HOST,
    USB_MODE_AUTO
};

// Configuration structure
typedef struct {
  // UART settings
  uint32_t baudrate;
  uint8_t databits;
  String parity;     // "none", "even", "odd"
  uint8_t stopbits;
  bool flowcontrol;
  
  // WiFi settings
  String ssid;
  String password;
  
  // System info
  String version;
  String device_name;

  // USB mode
  UsbMode usb_mode = USB_MODE_DEVICE;  // Now UsbMode is already defined above
} Config;

// Traffic statistics
typedef struct {
  unsigned long bytesUartToUsb;
  unsigned long bytesUsbToUart;
  unsigned long lastActivityTime;
  unsigned long deviceStartTime;
  unsigned long totalUartPackets;
} UartStats;

// System state
typedef struct {
  bool wifiAPActive;
  unsigned long wifiStartTime;
  volatile int clickCount;
  volatile unsigned long lastClickTime;
  volatile bool buttonPressed;
  volatile unsigned long buttonPressTime;
} SystemState;

// LED state
typedef struct {
  unsigned long lastDataLedTime;
  bool dataLedState;
} LedState;

// Flow control detection results
typedef struct {
  bool flowControlDetected;
  bool flowControlActive;
} FlowControlStatus;

// Spinlock for statistics critical sections
extern portMUX_TYPE statsMux;

// Inline functions for convenient critical section handling
inline void enterStatsCritical() {
    taskENTER_CRITICAL(&statsMux);
}

inline void exitStatsCritical() {
    taskEXIT_CRITICAL(&statsMux);
}

#endif // TYPES_H