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
    if (instance) {
        uint32_t now = micros();
        int currentLevel = digitalRead(instance->_rxPin);
        Serial.printf("GPIO Change ISR: level= %d after a duration of = %u µs\n", currentLevel, now - instance->lastEdgeTime);
        instance->lastEdgeTime = now;
        
        if (instance->edgeCount < MAX_EDGES) {
            instance->edgeTimes[instance->edgeCount++] = now;
        }
        
        // Stocker le niveau et signaler que la LED doit être mise à jour
        instance->_currentRxLevel = currentLevel;
        instance->_ledUpdateNeeded = true;
    }
}

void SeatalkGPIO::task() {
    uint32_t now = micros();
    uint32_t timeSinceLastEdge = now - lastEdgeTime;
    
    // Mettre à jour la LED si nécessaire (en dehors de l'ISR)
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
    
    // Attendre au moins 10 edges et 2ms de silence pour démarrer le décodage
    // (3 caractères = 11 bits × 3 = 33 bits, ~30 transitions minimum)
    if (edgeCount >= 10 && timeSinceLastEdge > 2000) {
        decodeSeatalkFrame();
        edgeCount = 0;
        delay(50);
    }
}

void SeatalkGPIO::decodeSeatalkFrame() {
    const int MAX_BITS = 512;
    uint8_t bitstream[MAX_BITS];
    int bitCount = 0;

    int current_level = 1; // idle HIGH

    Serial.print("\n=== Raw edges (");
    Serial.print(edgeCount);
    Serial.println(" total) ===");
    
    // Debug : afficher les durées
    for (int i = 1; i < edgeCount && i < 20; i++) {
        uint32_t duration = edgeTimes[i] - edgeTimes[i - 1];
        Serial.print("  Edge ");
        Serial.print(i);
        Serial.print(": ");
        Serial.print(duration);
        Serial.println("µs");
    }

    // 🔧 1. Reconstruction du bitstream à partir des durées
    for (int i = 1; i < edgeCount && bitCount < MAX_BITS; i++) {
        uint32_t duration = edgeTimes[i] - edgeTimes[i - 1];

        // Calculer le nombre de bits (multiples de ~208µs)
        int numBits = (duration + BIT_DURATION_US / 2) / BIT_DURATION_US;

        // 🔥 GAP très long (> 2000µs) = fin de trame, reset
        if (numBits > 20) {
            if (bitCount > 0) {
                break; // Traiter la trame accumulée
            }
            continue; // Ignorer les gaps au début
        }

        // Sécurité
        if (numBits < 1) numBits = 1;

        // ⚠️ IMPORTANT: La durée représente le PROCHAIN niveau (après la transition)
        // Donc on ajoute le niveau ACTUEL (avant la transition)
        for (int b = 0; b < numBits && bitCount < MAX_BITS; b++) {
            bitstream[bitCount++] = current_level;
        }

        // Basculer pour la prochaine transition
        current_level = 1 - current_level;
    }

    Serial.print("\nBitstream (");
    Serial.print(bitCount);
    Serial.print(" bits): ");
    for (int i = 0; i < bitCount && i < 150; i++) {
        Serial.print(bitstream[i]);
    }
    Serial.println();

    // 🔧 2. Découpe en caractères Seatalk (11 bits: 1 START + 8 DATA + 1 CMD + 1 STOP)
    uint8_t datagram[MAX_DATAGRAM_LENGTH];
    int charIndex = 0;

    for (int i = 0; i <= bitCount - 11 && charIndex < MAX_DATAGRAM_LENGTH; i++) {

        // Chercher un START bit (0) suivi d'un STOP bit (1) à la position correcte
        if (bitstream[i] != 0) continue; // Pas de START bit
        if (bitstream[i + 10] != 1) continue; // Pas de STOP bit

        uint16_t word = 0;

        // Extraire les 8 bits de données (positions 1-8) en LSB first
        for (int b = 0; b < 8; b++) {
            if (bitstream[i + 1 + b]) {
                word |= (1 << b); // LSB first
            }
        }

        // Inversion logique Seatalk (HIGH=0, LOW=1 sur le bus)
        word ^= 0xFF;

        uint8_t data = word & 0xFF;

        Serial.print("Char ");
        Serial.print(charIndex);
        Serial.print(": raw bits=");
        for (int b = 0; b < 11; b++) {
            Serial.print(bitstream[i + b]);
        }
        Serial.print(" = 0x");
        if (word < 0x100) Serial.print("0");
        Serial.print(word, HEX);
        Serial.print(" -> 0x");
        Serial.println(data, HEX);

        datagram[charIndex++] = data;

        // Avancer au prochain caractère potentiel (skip les 11 bits)
        i += 10;
    }

    Serial.print("\nDecoded ");
    Serial.print(charIndex);
    Serial.print(" bytes: ");
    for (int i = 0; i < charIndex; i++) {
        Serial.print("0x");
        if (datagram[i] < 0x10) Serial.print("0");
        Serial.print(datagram[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    if (charIndex >= 3 && _callback) {
        _callback(datagram, charIndex);
    }
}
