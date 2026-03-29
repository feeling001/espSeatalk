#include "seatalk_rmt.h"

SeatalkRMT::SeatalkRMT(int rxPin, int txPin,
                       rmt_channel_t rxChannel,
                       rmt_channel_t txChannel)
    : _rxPin(rxPin), _txPin(txPin),
      _rxChannel(rxChannel), _txChannel(txChannel) {}

void SeatalkRMT::begin() {

    Serial.print("Initializing RMT RX on pin ");
    Serial.print(_rxPin);
    Serial.print(" channel ");
    Serial.println(_rxChannel);

    // RX - configuration plus robuste pour ESP32-S3
    rmt_config_t rx = {};
    rx.rmt_mode = RMT_MODE_RX;
    rx.channel = _rxChannel;
    rx.gpio_num = (gpio_num_t)_rxPin;
    rx.clk_div = 80; // 1 tick = 1µs
    rx.mem_block_num = 1; // Réduire à 1 au lieu de 2

    rx.rx_config.filter_en = true;
    rx.rx_config.filter_ticks_thresh = 10; // Réduire le filtre
    rx.rx_config.idle_threshold = 2000; // Réduire l'idle threshold

    esp_err_t err = rmt_config(&rx);
    if (err != ESP_OK) {
        Serial.print("RMT RX config failed with error: ");
        Serial.println(err);
        Serial.println("Falling back to GPIO mode recommended");
        return;
    }

    err = rmt_driver_install(_rxChannel, 1024, 0);
    if (err != ESP_OK) {
        Serial.print("RMT RX driver install failed: ");
        Serial.println(err);
        return;
    }

    rmt_get_ringbuf_handle(_rxChannel, &_rb);
    
    err = rmt_rx_start(_rxChannel, true);
    if (err != ESP_OK) {
        Serial.print("RMT RX start failed: ");
        Serial.println(err);
        return;
    }

    Serial.println("RX initialized successfully");

    Serial.print("Initializing RMT TX on pin ");
    Serial.print(_txPin);
    Serial.print(" channel ");
    Serial.println(_txChannel);

    // TX
    rmt_config_t tx = {};
    tx.rmt_mode = RMT_MODE_TX;
    tx.channel = _txChannel;
    tx.gpio_num = (gpio_num_t)_txPin;
    tx.clk_div = 80;
    tx.mem_block_num = 1;

    tx.tx_config.loop_en = false;
    tx.tx_config.carrier_en = false;
    tx.tx_config.idle_output_en = true;
    tx.tx_config.idle_level = RMT_IDLE_LEVEL_HIGH;

    err = rmt_config(&tx);
    if (err != ESP_OK) {
        Serial.print("RMT TX config failed: ");
        Serial.println(err);
        return;
    }

    err = rmt_driver_install(_txChannel, 0, 0);
    if (err != ESP_OK) {
        Serial.print("RMT TX driver install failed: ");
        Serial.println(err);
        return;
    }

    Serial.println("TX initialized");
}

void SeatalkRMT::onReceive(seatalk_rx_callback_t cb) {
    _callback = cb;
}

void SeatalkRMT::task() {
    if (!_rb) return;

    size_t len = 0;
    rmt_item32_t* items = (rmt_item32_t*) xRingbufferReceive(_rb, &len, 0);

    if (items) {
        size_t item_count = len / sizeof(rmt_item32_t);
        
        uint16_t frame = decode(items, item_count);
        
        if (frame != 0 && _callback) {
            _callback(frame);
        }
        
        vRingbufferReturnItem(_rb, (void*) items);
    }
}

uint16_t SeatalkRMT::decode(rmt_item32_t* items, size_t count) {
    uint16_t data = 0;
    int bitIndex = 0;
    
    // Accumuler les durées et niveaux dans une séquence linéaire
    int current_level = 1; // HIGH au départ (idle)
    
    for (size_t i = 0; i < count && bitIndex < 11; i++) {
        // Première moitié du RMT item
        uint32_t duration = items[i].duration0;
        
        if (duration == 0) continue;
        
        int numBits = (duration + BIT_US / 2) / BIT_US;
        
        if (numBits > 3) numBits = 3;
        if (numBits < 1) numBits = 1;
        
        int level_during = current_level;
        
        for (int b = 0; b < numBits && bitIndex < 11; b++) {
            // HIGH = 0, LOW = 1 (logique inversée Seatalk)
            if (level_during == 0) {
                data |= (1 << bitIndex);
            }
            bitIndex++;
        }
        
        current_level = 1 - current_level;
        
        // Deuxième moitié du RMT item
        duration = items[i].duration1;
        
        if (duration == 0) continue;
        
        numBits = (duration + BIT_US / 2) / BIT_US;
        
        if (numBits > 3) numBits = 3;
        if (numBits < 1) numBits = 1;
        
        level_during = current_level;
        
        for (int b = 0; b < numBits && bitIndex < 11; b++) {
            if (level_during == 0) {
                data |= (1 << bitIndex);
            }
            bitIndex++;
        }
        
        current_level = 1 - current_level;
    }
    
    if (bitIndex >= 11) {
        return data;
    }
    
    return 0;
}

inline rmt_item32_t makeItem(uint16_t d0, uint8_t l0, uint16_t d1, uint8_t l1) {
    rmt_item32_t item;
    item.duration0 = d0;
    item.level0    = l0;
    item.duration1 = d1;
    item.level1    = l1;
    return item;
}

void SeatalkRMT::send(uint16_t data) {

    rmt_item32_t frame[11];

    // start bit (LOW)
    frame[0] = makeItem(BIT_US, 0, 0, 0);

    // 9 bits
    for (int i = 0; i < 9; i++) {
        bool bit = (data >> i) & 1;
        frame[i + 1] = makeItem(BIT_US, bit ? 0 : 1, 0, 0);
    }
    
    /* stop bit (HIGH) */
    frame[10] = makeItem(BIT_US, 1, 0, 0);

    rmt_write_items(_txChannel, frame, 11, true);
    rmt_wait_tx_done(_txChannel, pdMS_TO_TICKS(10));
}