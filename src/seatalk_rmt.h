#pragma once

#include <Arduino.h>
#include <driver/rmt.h>

typedef void (*seatalk_rx_callback_t)(uint16_t data);

class SeatalkRMT {
public:
    SeatalkRMT(int rxPin, int txPin,
               rmt_channel_t rxChannel = RMT_CHANNEL_0,
               rmt_channel_t txChannel = RMT_CHANNEL_1);

    void begin();
    void task();  // à appeler dans loop()
    void send(uint16_t data); // 9 bits

    void onReceive(seatalk_rx_callback_t cb);

private:
    int _rxPin;
    int _txPin;

    rmt_channel_t _rxChannel;
    rmt_channel_t _txChannel;

    RingbufHandle_t _rb = nullptr;

    seatalk_rx_callback_t _callback = nullptr;

    static const int BIT_US = 208;

    uint16_t decode(rmt_item32_t* items, size_t count);
};