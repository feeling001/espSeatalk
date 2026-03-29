#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

typedef void (*seatalk_rx_callback_t)(uint8_t* data, int length);

class SeatalkGPIO {
public:
    SeatalkGPIO(int rxPin, Adafruit_NeoPixel* ledPixels = nullptr);
    
    void begin();
    void task();
    void onReceive(seatalk_rx_callback_t cb);

private:
    static const int BIT_DURATION_US = 208;
    static const int MAX_EDGES = 250;
    static const int FRAME_TIMEOUT_US = 200000;
    static const int MAX_DATAGRAM_LENGTH = 18;
    
    int _rxPin;
    seatalk_rx_callback_t _callback;
    
    volatile uint32_t edgeTimes[MAX_EDGES];
    volatile int edgeCount;
    volatile uint32_t lastEdgeTime;
    
    Adafruit_NeoPixel* _pixels;
    volatile int _currentRxLevel;
    volatile bool _ledUpdateNeeded;
    
    static SeatalkGPIO* instance;
    
    static void gpioChangeISR();
    void decodeSeatalkFrame();
};
