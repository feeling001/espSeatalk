#include <Arduino.h>

#include "seatalk_rmt.h"

/*
#include "driver/rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

#include "esp_rom_gpio.h"
#include "soc/gpio_sig_map.h"
*/


#define RX_PIN            GPIO_NUM_8
#define TX_PIN            GPIO_NUM_7
#define RMT_RX_CHANNEL    RMT_CHANNEL_4 
#define RMT_TX_CHANNEL    RMT_CHANNEL_1 
/*
#define SEATALK_BIT_US    208
#define HALF_BIT_US       104
#define IDLE_THRESHOLD_US 3000 // 3ms de silence pour séparer les messages
*/

/*
uint8_t reverse8(uint8_t x) {
    x = (x >> 4) | (x << 4);
    x = ((x & 0xCC) >> 2) | ((x & 0x33) << 2);
    x = ((x & 0xAA) >> 1) | ((x & 0x55) << 1);
    return x;
}
*/
TaskHandle_t seatalkTaskHandle;

void seatalkTask(void* parameter) ;

SeatalkRMT seatalk1;
uint32_t lastSendTime;
uint8_t light;
uint16_t mcog;

void setup() {
    Serial.begin(115200);
    delay(2000);
    lastSendTime=0;
    seatalk1.init(RX_PIN,TX_PIN,RMT_RX_CHANNEL,RMT_TX_CHANNEL);
    mcog=0;
    BaseType_t seatalkResult = xTaskCreatePinnedToCore(seatalkTask, "SeaTalk", 4096, NULL, 5, &seatalkTaskHandle, 0);

}

void loop() {
    
    delay(5);
    

    if (millis() - lastSendTime >= 3000) {
        lastSendTime = millis();

        mcog++;
        if(mcog>360) mcog=0;

        // 53  U0  VW
        // The two lower  bits of  U * 90 +
        //          the six lower  bits of VW *  2 +
        //          the two higher bits of  U /  2 =
        //          (U & 0x3) * 90 + (VW & 0x3F) * 2 + (U & 0xC) / 8
        Serial.printf("send cog %u \n",mcog);
        uint8_t b1 =  ( (mcog/90) << 4 ) + ( (mcog%2)<<7 ) ;
        uint8_t b2 = (mcog%90)/2 ;
        uint8_t lightMsg[] = {0x53, b1, b2}; 
        seatalk1.sendDatagram(lightMsg, 3);




        /*
        uint8_t lightMsg[] = {0x30, 0x00, light}; 
        if ( seatalk1.sendDatagram(lightMsg, 3) ) {
            Serial.printf("Send success \n\n");
        }
        else {
            Serial.printf("Send failure \n\n");
        }

        if(light==0x00) light=0x04;
        else if(light==0x04) light=0x08;
        else if(light==0x08) light=0x0C;
        else if(light==0x0C) light=0x00;
        */

    }
        
}


// ═══════════════════════════════════════════════════════════════
// CORE 0: Seatalk Task
// ═══════════════════════════════════════════════════════════════
void seatalkTask(void* parameter) {
    Serial.printf("[SeaTalk] Started on Core 0\n");

    uint32_t lastStatsTime  = millis();
    uint32_t sentencesRead  = 0;
    uint32_t parseErrors    = 0;

    while (true) {
        seatalk1.task();
        vTaskDelay(1 / portTICK_PERIOD_MS);
        }
    }
