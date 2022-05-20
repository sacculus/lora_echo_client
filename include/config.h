#ifndef __CONFIG_H__
#define __CONFIG_H__

/*
 * Serial monitor output
 * If you want to write messages to serial monitor
 * set CORE_DEBUG_LEVEL in project build_flags to value great than 0
 * 
 * Notice: It is not recommend to relise builds
 */
#if CORE_DEBUG_LEVEL > 0

#define LOGGING

/* ESP-IDF logging */
#pragma message("Output to serial monitor is enabled, it is not recommend to relise builds!")
#include "esp32-hal-log.h"
#define LOG_V(...) log_v(##__VA_ARGS__)
#define LOG_D(...) log_d(##__VA_ARGS__)
#define LOG_I(...) log_i(##__VA_ARGS__)
#define LOG_W(...) log_w(##__VA_ARGS__)
#define LOG_E(...) log_e(##__VA_ARGS__)

#else
#define LOG_V(...)
#define LOG_D(...)
#define LOG_I(...)
#define LOG_W(...)
#define LOG_E(...)
#endif /* Serial monitor output */

/*
 * Board peripherial pin bindings
 */
#if defined(ARDUINO_LOLIN32)
/* Wemos Lolin32 */
#define LORA_PIN_SCK                GPIO_NUM_18
#define LORA_PIN_MISO               GPIO_NUM_19
#define LORA_PIN_MOSI               GPIO_NUM_23
#define LORA_PIN_NSS                 GPIO_NUM_5
#define LORA_PIN_RST                GPIO_NUM_17
#define LORA_PIN_DIO0                GPIO_NUM_4
#define LORA_PIN_DIO1                GPIO_NUM_X

#define SSD1306_PIN_SCL             GPIO_NUM_22
#define SSD1306_PIN_SDA             GPIO_NUM_21
#define SSD1306_PIN_RST          (gpio_num_t)-1     // is not used

#define BUTTON_PIN                   GPIO_NUM_0
#elif defined(ARDUINO_HELTEC_WIFI_LORA_32_V2)
/* HelTec WIFI LoRa 32 V2 board */
#define VEXT_CONTROL_PIN            GPIO_NUM_21

#define LORA_PIN_SCK                 GPIO_NUM_5
#define LORA_PIN_MISO               GPIO_NUM_19
#define LORA_PIN_MOSI               GPIO_NUM_27
#define LORA_PIN_NSS                GPIO_NUM_18
#define LORA_PIN_RST                GPIO_NUM_14
#define LORA_PIN_DIO0               GPIO_NUM_26
#define LORA_PIN_DIO1               GPIO_NUM_35

#define SSD1306_PIN_SCL             GPIO_NUM_15
#define SSD1306_PIN_SDA              GPIO_NUM_4
#define SSD1306_PIN_RST             GPIO_NUM_16

#define BUTTON_PIN                   GPIO_NUM_0     // PRG button
#else
#error Compilation eror: Board not supported
#error Supported boards: WEMOS LOLIN32, HelTec WiFi LoRa 32 V2
#endif

/*
 * LoRa settings
 */
#define LORA_FREQUENCY                    434.0
/* Max 10 dBm for 433 MHz and 14 dBm for 868 MHz */
#define LORA_TRANSMIT_POWER                  10
#define LORA_PREAMBLE_LENGTH                  8
#define LORA_SYNCWORD                      0x12

#define LORA_SPREADING_FACTOR                10
#define LORA_BANDWIDTH                    125.0
#define LORA_CODING_RATE                      5
/* TODO: Should be calculated on bitrate */
#define LORA_MAXIMUM_PAYLOAD_SIZE           256

typedef enum {
    STATE_IDLE = 0,
    STATE_SEND,
    STATE_RECEIVE,
    STATE_DISPLAY
} loop_state_t;

/* 
 * Display
 */
#define SSD1306_ADDRESS                    0x3C

#define DISPLAY_WIDTH                       128
#define DISPLAY_HEIGHT                       64

/*
 * Timers
 */
#define TIMER_HW_INTERVAL_US             1000UL

#define TIMER_SEND_TIMEOUT_MS             2000U
#define TIMER_RECEIVE_TIMEOUT_MS          5000U

#define TIMER_DISPLAY_DURATION_MS        10000U

/*
 * Buttons
 */
#define BUTTON_DEBOUNCE_INTERVAL             50
#define BUTTON_LONG_CLICK_INTERVAL         2000

#endif /* __CONFIG_H__ */