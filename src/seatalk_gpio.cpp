#include "seatalk_gpio.h"

SeatalkGPIO* SeatalkGPIO::instance = nullptr;

SeatalkGPIO::SeatalkGPIO(int rxPin, Adafruit_NeoPixel* ledPixels) 
    : _rxPin(rxPin), _callback(nullptr), edgeCount(0), lastEdgeTime(0), 
      _pixels(ledPixels), _currentRxLevel(1), _ledUpdateNeeded(false) {
    instance = this;
}

void SeatalkGPIO::begin() {
    pinMode(_rxPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(_rxPin), gpioChangeISR, CHANGE);
    Serial.print("SeatalkGPIO initialized on pin ");
    Serial.println(_rxPin);
}

void SeatalkGPIO::onReceive(seatalk_rx_callback_t cb) {
    _callback = cb;
}

void IRAM_ATTR SeatalkGPIO::gpioChangeISR() {
        uint32_t now = micros();
        int currentLevel = digitalRead(instance->_rxPin);
        uint32_t duration = now - instance->lastEdgeTime;
        Serial.printf("GPIO Change ISR: level= %d after a duration of = %u µs\n", currentLevel, now - instance->lastEdgeTime);
        instance->lastEdgeTime = now;
        if(duration > FRAME_TIMEOUT_US) {
            Serial.println(">>> New frame detected");
        }
        if(duration > 4800 ) {
            Serial.printf(" - %d, start          =  1\n", currentLevel);
        }
        else {
            Serial.printf(" - %d, (%6u µs)    =  %d x %d\n", currentLevel, duration, (currentLevel+1)%2 , (duration+104)/BIT_DURATION_US );
        }
}

void SeatalkGPIO::task() {
    uint32_t now = micros();
    uint32_t timeSinceLastEdge = now - lastEdgeTime;
    
    // Mettre à jour la LED si nécessaire (en dehors de l'ISR)
    /*
    if (_ledUpdateNeeded && _pixels) {
        _ledUpdateNeeded = false;
        
        if (_currentRxLevel == 1) {
            // RX HIGH -> LED VERT
            _pixels->setPixelColor(0, _pixels->Color(0, 120, 0));
        } else {
            // RX LOW -> LED ROUGE
            _pixels->setPixelColor(0, _pixels->Color(255, 0, 0));
        }
        _pixels->show();
    }
    */
}

void SeatalkGPIO::decodeSeatalkFrame() {
    Serial.println("Decoding Seatalk frame...");
}