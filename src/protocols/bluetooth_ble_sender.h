// Bluetooth BLE sender for ESP32-S3 (Nordic UART Service)
#if defined(BLE_ENABLED)

#ifndef BLUETOOTH_BLE_SENDER_H
#define BLUETOOTH_BLE_SENDER_H

#include "packet_sender.h"
#include "sbus_router.h"
#include "../bluetooth/bluetooth_ble.h"
#include "../device_types.h"
#include "../device_stats.h"
#include "../types.h"
#include "../logging.h"
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class BluetoothBLESender : public PacketSender {
private:
    uint32_t lastSendAttempt;
    uint32_t backoffDelay;
    uint32_t sendRateIntervalMs = 0;  // 0 = disabled (no rate limit)
    uint32_t lastRateSendMs = 0;
    SemaphoreHandle_t queueMutex;  // Protect queue from concurrent access

    void applyBackoff(uint32_t delayUs = 1000) {
        lastSendAttempt = micros();
        backoffDelay = (backoffDelay == 0) ? delayUs : std::min((uint32_t)(backoffDelay * 2), (uint32_t)5000);
    }

    void resetBackoff() {
        backoffDelay = 0;
    }

    bool inBackoff() const {
        return backoffDelay > 0 && (micros() - lastSendAttempt) < backoffDelay;
    }

    void commitPackets(size_t count) {
        for (size_t i = 0; i < count; i++) {
            if (packetQueue.empty()) break;
            totalSent++;

            ParsedPacket& pkt = packetQueue.front().packet;
            currentQueueBytes -= pkt.size;
            pkt.free();
            packetQueue.pop_front();
        }
    }

public:
    // BLE queue - smaller than BT SPP due to notify-based transmission
    static constexpr size_t BLE_MAX_PACKETS = 16;
    static constexpr size_t BLE_MAX_BYTES = 4096;

    BluetoothBLESender() :
        PacketSender(BLE_MAX_PACKETS, BLE_MAX_BYTES),
        lastSendAttempt(0),
        backoffDelay(0) {
        queueMutex = xSemaphoreCreateMutex();
        log_msg(LOG_DEBUG, "BluetoothBLESender initialized (queue: %d pkts, %d bytes)",
                BLE_MAX_PACKETS, BLE_MAX_BYTES);
    }

    ~BluetoothBLESender() {
        if (queueMutex) {
            vSemaphoreDelete(queueMutex);
        }
    }

    // Direct send without queue (for fast path - SBUS text)
    size_t sendDirect(const uint8_t* data, size_t size) override {
        if (!bluetoothBLE || !bluetoothBLE->hasClient()) return 0;

        // Rate limiting check (if enabled via setSendRate)
        if (sendRateIntervalMs > 0) {
            uint32_t now = millis();
            if ((now - lastRateSendMs) < sendRateIntervalMs) {
                return 0;  // Skip frame (rate limited)
            }
            lastRateSendMs = now;
        }

        const uint8_t* sendData = data;
        size_t sendSize = size;

        // Convert SBUS binary to text if TEXT format selected
        if (sbusOutputFormat == SBUS_FMT_TEXT && size == SBUS_FRAME_SIZE && data[0] == SBUS_START_BYTE) {
            char* buf = SbusRouter::getInstance()->getConvertBuffer();
            if (!buf) return 0;  // Buffer not allocated - skip
            sendSize = sbusFrameToText(data, buf, SBUS_OUTPUT_BUFFER_SIZE);
            if (sendSize == 0) return 0;
            sendData = (const uint8_t*)buf;
        }

        size_t sent = bluetoothBLE->write(sendData, sendSize);
        if (sent > 0) {
            g_deviceStats.device5.txBytes.fetch_add(sent, std::memory_order_relaxed);
            g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
        }
        return sent;
    }

    void processSendQueue(bool bulkMode = false) override {
        (void)bulkMode;  // BLE doesn't use bulk mode

        if (inBackoff()) return;
        if (!isReady()) return;

        // Lock queue for entire operation
        if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(5)) != pdTRUE) {
            return;  // Couldn't get lock, try next time
        }

        if (packetQueue.empty()) {
            xSemaphoreGive(queueMutex);
            return;
        }

        QueuedPacket* item = &packetQueue.front();

        // Safety check: skip corrupted packets
        if (!item->packet.data || item->packet.size == 0 || item->packet.size > 512) {
            log_msg(LOG_ERROR, "[BLE] Corrupt packet detected, dropping");
            if (item->packet.allocSize > 0) {
                currentQueueBytes -= item->packet.size;
                item->packet.free();
            }
            packetQueue.pop_front();
            totalDropped++;
            xSemaphoreGive(queueMutex);
            return;
        }

        // Send packet while holding lock
        size_t sent = bluetoothBLE->write(item->packet.data, item->packet.size);

        if (sent > 0) {
            resetBackoff();
            g_deviceStats.device5.txBytes.fetch_add(sent, std::memory_order_relaxed);
            g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
            commitPackets(1);
        } else {
            applyBackoff();
        }

        xSemaphoreGive(queueMutex);
    }

    bool isReady() const override {
        return bluetoothBLE && bluetoothBLE->hasClient();
    }

    const char* getName() const override {
        return "BLE";
    }

    // Override enqueue with mutex protection
    bool enqueue(const ParsedPacket& packet) override {
        // Reject invalid packets early
        if (!packet.data || packet.size == 0 || packet.size > 512) {
            return false;
        }

        // Lock queue for thread-safe access
        if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(5)) != pdTRUE) {
            totalDropped++;
            return false;
        }

        bool result = PacketSender::enqueue(packet);
        xSemaphoreGive(queueMutex);
        return result;
    }

    // Set send rate limit for SBUS text mode
    void setSendRate(uint8_t rateHz) {
        if (rateHz == 0) {
            sendRateIntervalMs = 0;  // Disabled
        } else {
            sendRateIntervalMs = 1000 / rateHz;
            log_msg(LOG_INFO, "BLE send rate: %d Hz (%lu ms interval)", rateHz, sendRateIntervalMs);
        }
    }
};

#endif // BLUETOOTH_BLE_SENDER_H
#endif // BLE_ENABLED
