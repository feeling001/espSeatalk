#include <Arduino.h>
#include "seatalk_gpio.h"
#include <Adafruit_NeoPixel.h>

#define RX_PIN 8
#define TX_PIN 7
#define LED_PIN 21
#define LED_COUNT 1

Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
SeatalkGPIO seatalk(RX_PIN, &pixels);

unsigned long lastSendTime = 0;

// Variables pour monitoring RX
volatile uint32_t rxEdgeTimes[100];
volatile int rxEdgeCount = 0;

void printRxEdges() {
    if (rxEdgeCount > 0) {
        Serial.print("\n=== RX Edges (");
        Serial.print(rxEdgeCount);
        Serial.println(" total) ===");
        
        for (int i = 1; i < rxEdgeCount && i < 50; i++) {
            uint32_t duration = rxEdgeTimes[i] - rxEdgeTimes[i - 1];
            int level = (i % 2 == 1) ? 0 : 1;
            
            Serial.print("  Edge ");
            Serial.print(i);
            Serial.print(": ");
            Serial.print(duration);
            Serial.print("µs, level after: ");
            Serial.println(level ? "HIGH" : "LOW");
        }
    }
}

void onSeatalk(uint8_t* data, int length) {
    if (length < 3) {
        Serial.println("Invalid datagram (too short)");
        return;
    }
    
    uint8_t command = data[0];
    uint8_t attr = data[1];
    uint8_t expectedLength = 3 + (attr & 0x0F);
    
    Serial.println();
    Serial.print("=== Seatalk Datagram ===");
    Serial.println();
    Serial.print("Command: 0x");
    Serial.print(command, HEX);
    Serial.print(" [");
    for (int b = 7; b >= 4; b--) Serial.print((command >> b) & 1);
    Serial.print("|");
    for (int b = 3; b >= 0; b--) Serial.print((command >> b) & 1);
    Serial.println("]");
    
    Serial.print("Attribute: 0x");
    Serial.print(attr, HEX);
    Serial.print(" [");
    for (int b = 7; b >= 4; b--) Serial.print((attr >> b) & 1);
    Serial.print("|");
    for (int b = 3; b >= 0; b--) Serial.print((attr >> b) & 1);
    Serial.print("] (Length nibble: ");
    Serial.print(attr & 0x0F);
    Serial.println(")");
    
    Serial.print("Expected length: ");
    Serial.print(expectedLength);
    Serial.print(" bytes, Received: ");
    Serial.print(length);
    Serial.println(" bytes");
    
    if (length >= expectedLength) {
        Serial.print("Data bytes (");
        Serial.print(length - 2);
        Serial.println("):");
        for (int i = 2; i < length; i++) {
            Serial.print("  [");
            Serial.print(i - 1);
            Serial.print("] 0x");
            if (data[i] < 0x10) Serial.print("0");
            Serial.print(data[i], HEX);
            Serial.print(" [");
            for (int b = 7; b >= 4; b--) Serial.print((data[i] >> b) & 1);
            Serial.print("|");
            for (int b = 3; b >= 0; b--) Serial.print((data[i] >> b) & 1);
            Serial.println("]");
        }
    } else {
        Serial.println("WARNING: Incomplete datagram!");
    }
    
    Serial.print("Full datagram: ");
    for (int i = 0; i < length; i++) {
        Serial.print("0x");
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    Serial.println();
}

void sendSeatalkDatagram(uint8_t cmd, uint8_t attr, uint8_t data1) {
    uint8_t datagram[] = {cmd, attr, data1};
    
    Serial.print("\n>>> TX Datagram: 0x");
    Serial.print(cmd, HEX);
    Serial.print(" 0x");
    Serial.print(attr, HEX);
    Serial.print(" 0x");
    Serial.println(data1, HEX);
    
    // Réinitialiser le monitoring
    rxEdgeCount = 0;
    
    // Pour chaque byte du datagramme
    for (int byteIdx = 0; byteIdx < 3; byteIdx++) {
        uint8_t byte = datagram[byteIdx];
        
        // Start bit (0 = LOW)
        digitalWrite(TX_PIN, LOW);
        delayMicroseconds(208);
        
        // 8 data bits (LSB FIRST)
        for (int b = 0; b < 8; b++) {
            int bit = (byte >> b) & 1;
            digitalWrite(TX_PIN, bit ? HIGH : LOW);
            delayMicroseconds(208);
        }
        
        // Command bit
        int cmdBit = (byteIdx == 0) ? 1 : 0;
        digitalWrite(TX_PIN, cmdBit ? HIGH : LOW);
        delayMicroseconds(208);
        
        // Stop bit
        digitalWrite(TX_PIN, HIGH);
        delayMicroseconds(208);
        
        delay(5);
    }
    
    // Retour au repos
    digitalWrite(TX_PIN, HIGH);
    
    Serial.println("<<< TX completed");
    
    // Attendre un peu et afficher les edges reçus
    delay(50);
    printRxEdges();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\nSeatalk GPIO Interface (ESP32-S3)");
    Serial.println("Receiving and Transmitting Seatalk datagrams...");
    Serial.println("WS2812 LED on GPIO 21");
    Serial.println();
    
    // Initialiser la LED WS2812
    pixels.begin();
    pixels.setBrightness(100);
    pixels.setPixelColor(0, pixels.Color(0, 0, 160)); // LED BLEU au démarrage
    pixels.show();
    
    pinMode(TX_PIN, OUTPUT);
    digitalWrite(TX_PIN, HIGH);
    
    seatalk.begin();
    seatalk.onReceive(onSeatalk);
    
    lastSendTime = millis();
}

void loop() {
    seatalk.task();
    
    // Envoyer le datagramme toutes les 5 secondes
    if (millis() - lastSendTime >= 10000) {
        lastSendTime = millis();
        
        uint8_t lampValues[] = {0x00, 0x04, 0x08, 0x0C};
        static int levelIndex = 0;
        
        uint8_t lampLevel = lampValues[levelIndex];
        levelIndex = (levelIndex + 1) % 4;
        
        // Changer luminosité seatalk et envoyer le datagramme
        // sendSeatalkDatagram(0x30, 0x00, lampLevel);
        
        Serial.print("\n>>> TX Datagram: 0x53 0x20 0x08");
        Serial.println();       
        rxEdgeCount = 0;
        /* 

        53  U0  VW      Course over Ground (COG) in degrees:
                 The two lower  bits of  U * 90 +
                    the six lower  bits of VW *  2 +
                    the two higher bits of  U /  2 =
                    (U & 0x3) * 90 + (VW & 0x3F) * 2 + (U & 0xC) / 8 */
        sendSeatalkDatagram(0x53, 0x20, lampLevel);

        delay(100);
    }
    
    delay(10);
}