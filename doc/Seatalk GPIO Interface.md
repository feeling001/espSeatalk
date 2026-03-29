# Seatalk GPIO Interface - ESP32-S3 Implementation

## Overview

Cette implémentation permet de recevoir et d'envoyer des datagrammes **Seatalk** (protocole de communication navale) via GPIO sur un ESP32-S3.

### Status
✅ **FONCTIONNEL** - Réception et transmission validées sur Waveshare ESP32-S3-Zero

## Hardware

### Composants
- **Carte** : Waveshare ESP32-S3-Zero
- **Pin RX** : GPIO 8
- **Pin TX** : GPIO 7
- **Circuit d'interface** : Diviseur de tension (LM393) pour adapter les niveaux Seatalk (+12V) vers ESP32 (3.3V)

### Connexions Seatalk
Le protocole Seatalk utilise 3 fils :
1. **+12V** (rouge) - Alimentation
2. **GND** (gris) - Masse
3. **Data** (jaune) - Signal série

**Important** : Seatalk utilise une logique WIRED-OR (open-drain) :
- **+12V (idle/mark = 1)** : État repos, pullup interne ramène le bus à +12V
- **0V (space/data = 0)** : Quand un device tire le bus à LOW (pull-down)

## Protocol Details

### Format Serial (11 bits par caractère)
```
Start bit  : 0V (LOW)                    - 1 bit
Data bits  : LSB first (bit0...bit7)     - 8 bits  
Command bit: 1 for first char, 0 others - 1 bit
Stop bit   : +12V (HIGH)                 - 1 bit
─────────────────────────────────────────────────
Total      : 11 bits à 208µs chacun = ~2.3ms/char
```

### Timing
- **Baud rate** : 4800 baud
- **Bit duration** : 208µs (1/4800 = 208.33µs)
- **Délai inter-caractères** : ~5ms

### Structure du Datagramme
```
Byte 0   : Command (seul byte avec command bit = 1)
Byte 1   : Attribute
           - High nibble (bits 7-4): données ou 0
           - Low nibble (bits 3-0): n = nombre de bytes additionnels
           - Total datagram = 3 + n bytes
Byte 2...: Data bytes
```

### Exemple : Lampe (0x30)
```
Datagramme : 0x30 0x00 0x0A

0x30 = Lamp Status Command
0x00 = Attribute (0 bytes additionnels → total 3 bytes)
0x0A = Lampe level (0x00=OFF, 0x04=Low, 0x08=Med, 0x0C=High)
```

## Code Architecture

### Files
```
src/
├── main.cpp              - Programme principal (RX/TX)
├── seatalk_gpio.h        - Header classe GPIO
├── seatalk_gpio.cpp      - Implémentation GPIO
├── seatalk_rmt.h         - Header classe RMT (backup)
└── seatalk_rmt.cpp       - Implémentation RMT (backup)
```

### Classe SeatalkGPIO

#### Réception
- **ISR** : `onGpioChange()` en IRAM pour capturer chaque transition
- **Stockage** : Buffer de 250 transitions (jusqu'à 18+ caractères)
- **Timeout** : 200ms sans transition = fin de trame
- **Décodage** : 
  - Calcul nombre de bits par transition : `duration / 208µs`
  - Reconstruction des 8 bits (LSB first)
  - Inversion des bits reçus (HIGH=0, LOW=1 → ordre normal)

#### Transmission
- **Timing précis** : `delayMicroseconds(208)`
- **LSB First** : Boucle `for (int b = 0; b < 8; b++) { bit = (byte >> b) & 1 }`
- **Command bit** : 1 pour premier byte, 0 pour les autres
- **Logique Seatalk** : 
  - Bit=1 → HIGH (+12V)
  - Bit=0 → LOW (0V/pull-down)

## Key Implementation Details

### ⚠️ CRITICAL: LSB First

Seatalk envoie le **Least Significant Bit en premier** :
- Byte `0x30` en binaire normal : `00110000` (MSB first)
- Envoyé sur le bus : `00001100` (LSB first)

**À la réception** : Inverser les bits
```cpp
uint8_t reversed = 0;
for (int b = 0; b < 8; b++) {
    if (raw & (1 << b)) {
        reversed |= (1 << (7 - b));
    }
}
```

**À l'émission** : Envoyer LSB first directement
```cpp
for (int b = 0; b < 8; b++) {
    int bit = (byte >> b) & 1;  // b=0 = LSB
    digitalWrite(TX_PIN, bit ? HIGH : LOW);
}
```

### Détection de fin de trame
- **Condition** : 50+ transitions ET 200ms sans nouvelle transition
- **Raison** : Entre les caractères d'un datagramme, il y a des silences de ~1.6ms
  - Après le datagramme complet : silence > 200ms
  - Avant le prochain datagramme

### Pas d'inversion de logique à la réception

Contrairement à ce qu'on pourrait penser :
- **HIGH = 0** ✗ FAUX
- **HIGH = 1** ✓ VRAI
- Le signal reçu suit la logique Seatalk standard

Les bits reçus doivent être inversés (LOW=1, HIGH=0) car c'est ainsi que Seatalk code les données sur le bus.

## Utilisation

### Réception
```cpp
void onSeatalk(uint8_t* data, int length) {
    // data[0] = Command
    // data[1] = Attribute (length nibble = data[1] & 0x0F)
    // data[2..n] = Data bytes
}

SeatalkGPIO seatalk(RX_PIN);
seatalk.begin();
seatalk.onReceive(onSeatalk);

void loop() {
    seatalk.task();  // À appeler dans la loop()
}
```

### Transmission
```cpp
void sendSeatalkDatagram(uint8_t cmd, uint8_t attr, uint8_t data1) {
    // Envoie 3 bytes avec timing correct
}

// Exemple : Allumer lampe à niveau medium
sendSeatalkDatagram(0x30, 0x00, 0x08);
```

## Testing

### Vérification à l'oscilloscope
✅ Signal bien formé avec alternances HIGH/LOW
✅ Durée de bit ≈ 208µs
✅ Start bit = LOW, Stop bit = HIGH

### Données reçues validées
✅ `0x30 0x00 0x0A` = Lampe OFF
✅ `0x30 0x00 0x04` = Lampe Low
✅ `0x30 0x00 0x08` = Lampe Medium
✅ `0x30 0x00 0x0C` = Lampe High

## Fichiers Alternatifs (Non utilisés)

### seatalk_rmt.* (RMT - Remote Control Module)
- ❌ Problèmes de compatibilité ESP32-S3 avec API ancienne
- ⚠️ Erreur `RMT CHANNEL ERR` sur tous les channels testés
- 💾 Conservé en backup pour future migration IDF

### seatalk_gpio.* (GPIO - Actuellement utilisé)
- ✅ Fonctionnel et fiable
- ✅ Timing précis avec interruptions
- ✅ Pas de dépendance RMT
- **Recommandé** pour Seatalk sur ESP32-S3

## Performance

- **CPU** : Faible utilisation (ISR minimaliste)
- **Latence RX** : < 1ms après dernier bit
- **Latence TX** : 3 × 11 × 208µs = ~6.9ms par datagramme
- **Mémoire** : ~1KB pour buffers

## Références

- [Seatalk Protocol Reference (Part 1)](http://www.thomasknauf.de/rap/seatalk1.htm)
- [Seatalk Implementation Details (Part 3)](http://www.thomasknauf.de/rap/seatalk3.htm)
- [Thomas Knauf - Seatalk Documentation](http://www.thomasknauf.de/rap/seatalk.htm)

## Conclusion

Cette implémentation GPIO offre une solution **simple, robuste et fiable** pour communiquer avec des appareils Seatalk sur ESP32-S3, sans dépendance RMT problématique.

Le timing précis et la gestion LSB-first sont critiques pour la compatibilité Seatalk.