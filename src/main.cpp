#include <Arduino.h>
#include "config.h"
// #include <heltec.h>
/* Timers */
#include <ESP32_New_TimerInterrupt.h>
#include <ESP32_New_ISR_Timer.h>
/* LoRa */
#include <SPI.h>
#include <RadioLib.h>
/* Dispaly */
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
/* Buttons */
#include <Button2.h>
/* ESP32 RTC */
#include <driver/rtc_io.h>

ESP32Timer ITimer(1);

SX1278 LoRa = new Module(LORA_PIN_NSS, LORA_PIN_DIO0, LORA_PIN_RST, LORA_PIN_DIO1, SPI);

Adafruit_SSD1306 display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, SSD1306_PIN_RST);

Button2 button(BUTTON_PIN);

TaskHandle_t  task_button_click_handler, task_button_long_click_handler, task_lora_dio0_handler;

RTC_DATA_ATTR loop_state_t loop_state = STATE_IDLE;

RTC_DATA_ATTR uint32_t ping_data = 0, counter = 0, cnt_sent = 0, cnt_received = 0;
size_t data_len;
RTC_DATA_ATTR char buffer_out[LORA_MAXIMUM_PAYLOAD_SIZE];
char buffer_in[LORA_MAXIMUM_PAYLOAD_SIZE];

RTC_DATA_ATTR uint8_t display_buffer[DISPLAY_HEIGHT * DISPLAY_WIDTH / 8];

void goToSleep(uint64_t sleepDelayMs, bool stopLoRa, bool stopDisplay);

bool IRAM_ATTR ITimerHandler(void * timerNo)
{
    button.loop();
    return true;
}

void IRAM_ATTR buttonClickHandler(Button2 &btn)
{
    xTaskResumeFromISR(task_button_click_handler);
}

void IRAM_ATTR buttonLongClickHandler(Button2 &btn)
{
    xTaskResumeFromISR(task_button_long_click_handler);
}

void IRAM_ATTR loraDIO0Handler()
{
    xTaskResumeFromISR(task_lora_dio0_handler);
}

void button_click_handler(void *pParams)
{
    while (true)
    {
        vTaskSuspend(NULL);

        loop_state = STATE_DISPLAY;

        log_d("Display state, going to idle");

        /* Go to sleep */
        goToSleep(TIMER_DISPLAY_DURATION_MS, false, true);
    }

}

void button_long_click_handler(void *pParams)
{
    while (true)
    {
        vTaskSuspend(NULL);

        display.clearDisplay();
        display.setFont(&FreeMono9pt7b);
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        display.setCursor(0, 32);
        display.write("Send data");
        display.display();

        /* LoRa begin */
        LoRa.begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREADING_FACTOR, LORA_CODING_RATE, LORA_SYNCWORD, LORA_TRANSMIT_POWER);
        LoRa.setDio0Action(loraDIO0Handler);
        log_d("LoRa initialized");

        /* Payload to send */
        ping_data++;
        data_len = sprintf(buffer_out, "ping %d", ping_data);
        log_d("Send data: '%s' (%d bytes)", buffer_out, data_len);

        /* Send packet */
        if (RADIOLIB_ERR_NONE == LoRa.startTransmit((uint8_t *)buffer_out, data_len, 0)) {
            loop_state = STATE_SEND;

            log_d("Send OK, waiting to have sent");

            /* Go to sleep */
            goToSleep(TIMER_SEND_TIMEOUT_MS, true, true);
            continue;
        }
        display.setCursor(0, 48);
        display.write("error...");
        display.display();

        loop_state = STATE_DISPLAY;

        log_d("Send failed, going to idle");

        /* Go to sleep */
        goToSleep(TIMER_DISPLAY_DURATION_MS, false, true);
    }

}

void lora_dio0_handler(void *pParams)
{
    uint8_t len = 0, i;
    char *rssi, *snr, *fe;
    float in_RSSI, in_SNR, in_FERROR;
    int16_t x, y;
    uint16_t pos_x, pos_y, w, h;

    while (true)
    {
        vTaskSuspend(NULL);

        log_d("LoRa: DIO0 event");

        switch (loop_state)
        {
        case STATE_SEND:
            log_d("LoRa: Send done, going to receive");

            /* Increment count of sent packets */
            cnt_sent++;

            /* Display mode */
            display.clearDisplay();
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
            display.setFont(&FreeMono9pt7b);
            display.setCursor(0, 32);
            display.write("Receive");
            display.display();

            /* LoRa begin */
            LoRa.begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREADING_FACTOR, LORA_CODING_RATE, LORA_SYNCWORD, LORA_TRANSMIT_POWER);
            LoRa.setDio0Action(loraDIO0Handler);
            log_d("LoRa initialized");

            if (RADIOLIB_ERR_NONE == LoRa.startReceive())
            {
                /* Switch to receive mode */
                loop_state = STATE_RECEIVE;

                /* Display count of sent packets */
                display.fillRect(0, 0, 64, 16, SSD1306_BLACK);
                display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
                display.setFont(&FreeMonoBold9pt7b);
                display.getTextBounds(String(cnt_sent), 0, 0, &x, &y, &w, &h);
                /* x = 2 pixel offset to left from center, align right */
                /* y = center of 16 pixel's row */
                display.setCursor(DISPLAY_WIDTH / 2 - w - 3, (16 - h) / 2 + h - 1);
                display.printf("%d", cnt_sent);
                display.display();

                log_d("Receive started, going to sleep");

                /* Go to sleep */
                goToSleep(TIMER_RECEIVE_TIMEOUT_MS, true, true);
            }
            display.setCursor(0, 48);
            display.write("error...");
            display.display();

            loop_state = STATE_DISPLAY;

            log_d("Receive failed, going to idle");

            /* Go to sleep */
            goToSleep(TIMER_DISPLAY_DURATION_MS, false, true);
            break;
        case STATE_RECEIVE:
            len = LoRa.getPacketLength(true);
            if (len > 0 )
            {
                log_d("LoRa: Received data %d bytes", len);

                digitalWrite(LORA_PIN_NSS, LOW);
                in_RSSI = LoRa.getRSSI(true);
                in_SNR = LoRa.getSNR();
                in_FERROR = LoRa.getFrequencyError(false);
                digitalWrite(LORA_PIN_NSS, HIGH);

                String buff;
                digitalWrite(LORA_PIN_NSS, LOW);
                LoRa.readData((uint8_t *)buffer_in, len);
                digitalWrite(LORA_PIN_NSS, HIGH);
                buffer_in[len] = '\0';
                log_d("Got reply: '%s'", buffer_in);
                log_d("RSSI: %0.2f, SNR: %0.2f, Frequency error: %0.2f",
                        in_RSSI,
                        in_SNR,
                        in_FERROR);

                if (buffer_in == strstr(buffer_in, buffer_out))
                {
                    cnt_received++;

                    /* Parce received values */
                    rssi = strstr(buffer_in, "|");
                    rssi[0] = '\0';
                    rssi++;
                    snr = strstr(rssi, "|");
                    snr[0] = '\0';
                    snr++;
                    fe = strstr(snr, "|");
                    fe[0] = '\0';
                    fe++;
                
                    /* Clean display - area of received pakets count and main field */
                    display.fillRect(64, 0, DISPLAY_WIDTH / 2, 16, SSD1306_BLACK);
                    display.fillRect(0, 16, DISPLAY_WIDTH, 48, SSD1306_BLACK);

                    /* Count of received packets */
                    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
                    display.setFont(&FreeMonoBold9pt7b);
                    display.getTextBounds(String(cnt_received), 0, 0, &x, &y, &w, &h);
                    /* x = 2 pixel offset from right side, align right */
                    /* y = center of 16 pixel row */
                    display.setCursor(DISPLAY_WIDTH - w - 3, (16 - h) / 2 + h - 1);
                    display.printf("%d", cnt_received);

                    /* Statistic of packets */
                    display.setFont(&FreeMono9pt7b);
                    /* Server RSSI */
                    display.getTextBounds(rssi, 0, 0, &x, &y, &w, &h);
                    display.setCursor(DISPLAY_WIDTH / 2 - w - 3, 16 + h);
                    display.write(rssi);
                    /* Client RSSI */
                    display.getTextBounds(String(in_RSSI, 0), 0, 0, &x, &y, &w, &h);
                    display.setCursor(DISPLAY_WIDTH - w - 1, 16 + h);
                    display.printf("%0.0f", in_RSSI);
                    /* Server SNR */
                    display.getTextBounds(snr, 0, 0, &x, &y, &w, &h);
                    display.setCursor(DISPLAY_WIDTH / 2 - w - 3, 32 + h);
                    display.write(snr);
                    /* Client SNR */
                    display.getTextBounds(String(in_SNR, 2), 0, 0, &x, &y, &w, &h);
                    display.setCursor(DISPLAY_WIDTH - w - 3, 32 + h);
                    display.printf("%0.2f", in_SNR);
                    /* Server frequency error */
                    display.getTextBounds(fe, 0, 0, &x, &y, &w, &h);
                    display.setCursor(DISPLAY_WIDTH / 2 - w - 3, 48 + h);
                    display.write(fe);
                    /* Client frequency error */
                    display.getTextBounds(String(in_FERROR, 0), 0, 0, &x, &y, &w, &h);
                    display.setCursor(DISPLAY_WIDTH - w - 3, 48 + h);
                    display.printf("%0.0f", in_FERROR);
                    display.display();

                    loop_state = STATE_DISPLAY;

                    log_e("LoRa: Receive done, going to sleep");

                    /* Go to sleep */
                    goToSleep(TIMER_DISPLAY_DURATION_MS, false, true);
                }
                display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
                display.setFont(&FreeMono9pt7b);
                display.setCursor(0, 48);
                display.write("error...");
                display.display();

                loop_state = STATE_DISPLAY;

                log_e("LoRa: Received wrong packet '%s'", buffer_in);

                /* Go to sleep */
                goToSleep(TIMER_DISPLAY_DURATION_MS, false, true);
            }
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
            display.setFont(&FreeMono9pt7b);
            display.setCursor(0, 48);
            display.write("error...");
            display.display();

            loop_state = STATE_DISPLAY;

            log_e("LoRa: Received data has wrong size %d", len);

            /* Go to sleep */
            goToSleep(TIMER_DISPLAY_DURATION_MS, false, true);
            break;
        }
    }
}

void goToSleep(uint64_t sleepDurationMS = 0U, bool holdLoRa = false, bool holdDisplay = false)
{
    /* Hold LoRa gpios state */
    if (!holdLoRa)
        LoRa.sleep();
    gpio_hold_en(LORA_PIN_SCK);
    gpio_hold_en(LORA_PIN_MOSI);
    gpio_hold_en(LORA_PIN_MISO);
    gpio_hold_en(LORA_PIN_NSS);
    gpio_hold_en(LORA_PIN_RST);

    /* Hold display gpios & buffer state */
    if (!holdDisplay)
    {
        display.ssd1306_command(SSD1306_DISPLAYOFF);
    }
    else
    {
        /* Backup display buffer */
        memcpy(display_buffer, display.getBuffer(), DISPLAY_WIDTH * DISPLAY_HEIGHT / 8);
    }
    gpio_hold_en(SSD1306_PIN_SCL);
    gpio_hold_en(SSD1306_PIN_SDA);
#ifdef SSD1306_PIN_RST
    if (SSD1306_PIN_RST >= 0)
        gpio_hold_en(SSD1306_PIN_RST);
#endif

#ifdef VEXT_CONTROL_PIN
    if (holdLoRa || holdDisplay)
    {
        gpio_hold_en(VEXT_CONTROL_PIN);
    }
#endif
    gpio_deep_sleep_hold_en();

    /* Because external sources have different level we use EXT0&EXT1 sources together */
    /* Button */
    if (loop_state == STATE_IDLE || loop_state == STATE_DISPLAY)
    {
        esp_sleep_enable_ext0_wakeup(BUTTON_PIN, LOW);
    }
    /* LoRa */
    if (loop_state == STATE_SEND || loop_state == STATE_RECEIVE)
    {
        esp_sleep_enable_ext1_wakeup(1 << LORA_PIN_DIO0, ESP_EXT1_WAKEUP_ANY_HIGH);
    }
    if (sleepDurationMS > 0)
    {
        esp_sleep_enable_timer_wakeup((uint64_t)sleepDurationMS * 1000);
    }
    // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO);
    esp_deep_sleep_start();
}

void setup() {
#ifdef LOGGING
    Serial.begin(115200);
    while (!Serial);
#endif

    log_d("LoRa client setup");

    /* Wakeup cause - power on, waken on by gpio, timer etc */
    esp_sleep_wakeup_cause_t wakeup_reason;
    uint64_t wakeup_ext1_status = 0U;
    
    wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason)
    {
    case ESP_SLEEP_WAKEUP_EXT0:
        log_v("Wakeup caused by external signal using RTC_IO");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        wakeup_ext1_status = esp_sleep_get_ext1_wakeup_status();
        log_v("Wakeup caused by external signal using RTC_CNTL: %d",
                (log(wakeup_ext1_status))/log(2));
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        log_v("Wakeup caused by timer");
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        log_v("Wakeup caused by touchpad");
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        log_v("Wakeup caused by ULP program");
        break;
    default:
        log_v("Wakeup was not caused by deep sleep: %d", wakeup_reason);
        break;
    }

    /* Setup or unhold&restore gpios */
    if (wakeup_reason != ESP_SLEEP_WAKEUP_UNDEFINED)
    {
        /* Unhold gpios */
        gpio_hold_dis(LORA_PIN_SCK);
        gpio_hold_dis(LORA_PIN_MOSI);
        gpio_hold_dis(LORA_PIN_MISO);
        gpio_hold_dis(LORA_PIN_NSS);
        gpio_hold_dis(LORA_PIN_RST);
        gpio_hold_dis(SSD1306_PIN_SCL);
        gpio_hold_dis(SSD1306_PIN_SDA);
        gpio_hold_dis(SSD1306_PIN_RST);
        gpio_hold_dis(VEXT_CONTROL_PIN);
        gpio_deep_sleep_hold_dis();
    }
    else
    {
        /* Inital state - welcome message on display */
        loop_state = STATE_DISPLAY;
    }
    
    /* Enable Vext line, vext control (gpio21) pull down */
    pinMode(VEXT_CONTROL_PIN, OUTPUT);
    /* Power on VExt line */
    digitalWrite(VEXT_CONTROL_PIN, LOW);
    vTaskDelay(5 / portTICK_PERIOD_MS);

    /* Tasks */
    xTaskCreate(button_click_handler, "button_click_handler", 2000, NULL, tskIDLE_PRIORITY + 1, &task_button_click_handler);
    xTaskCreate(button_long_click_handler, "button_long_click_handler", 2000, NULL, tskIDLE_PRIORITY + 1, &task_button_long_click_handler);
    xTaskCreate(lora_dio0_handler, "lora_dio0_handler", 2000, NULL, tskIDLE_PRIORITY + 3, &task_lora_dio0_handler);
    log_d("Tasks initialized");

    /* SPI bus */
    SPI.begin(LORA_PIN_SCK, LORA_PIN_MISO, LORA_PIN_MOSI, LORA_PIN_NSS);

    /* LoRa */
    if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED)
    {
        /* LoRa begin */
        LoRa.begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREADING_FACTOR, LORA_CODING_RATE, LORA_SYNCWORD, LORA_TRANSMIT_POWER);
        LoRa.setDio0Action(loraDIO0Handler);
    }
    else
    {
        pinMode(LORA_PIN_RST, OUTPUT);
        digitalWrite(LORA_PIN_RST, HIGH);
        pinMode(LORA_PIN_DIO0, INPUT);
#ifdef LORA_PIN_DIO1
        pinMode(LORA_PIN_DIO1, INPUT);
#endif
        pinMode(LORA_PIN_NSS, OUTPUT);
        digitalWrite(LORA_PIN_NSS, HIGH);
    }
    log_d("LoRa initialized");

    /*
    * Display
    */
    Wire.begin(SSD1306_PIN_SDA, SSD1306_PIN_SCL, 400000U);

    if(!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_ADDRESS, (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED), false)) {
        log_e("SSD1306 allocation failed");
        while (1) ;
    }
#ifdef SSD1306_PIN_RST
    /* Fix SSD1306 RES line in high state (external pullup resistor) */
    if (SSD1306_PIN_RST >= 0)
    {
        pinMode(SSD1306_PIN_RST, INPUT);
    }
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(0xFF);
#endif
    if (wakeup_reason != ESP_SLEEP_WAKEUP_UNDEFINED)
    {
        /* Restore display buffer */
        memcpy(display.getBuffer(), display_buffer, DISPLAY_WIDTH * DISPLAY_HEIGHT / 8);
        display.display();
    }
    log_d("Display initialized");

    /* 
     * Buttons
     */
    button.setDebounceTime(BUTTON_DEBOUNCE_INTERVAL);
    button.setClickHandler(buttonClickHandler);
    button.setLongClickTime(BUTTON_LONG_CLICK_INTERVAL);
    button.setLongClickHandler(buttonLongClickHandler);
    log_d("Buttons initialized");

    /*
    * Timer
    */
    ITimer.attachInterruptInterval(TIMER_HW_INTERVAL_US, ITimerHandler);
    log_d("Timer initialized");

    log_d("LoRa client setup compleete");

    switch(wakeup_reason)
    {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
        /* Display welcome message */
        display.clearDisplay();
        display.setFont(&FreeMono9pt7b);
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        display.setCursor(0, 16);
        display.write("Press PRG\nbutton to\nsend...");
        display.display();
        
        /* Go to sleep */
        loop_state = STATE_DISPLAY;
        goToSleep(TIMER_DISPLAY_DURATION_MS, false, true);
        break;
    case ESP_SLEEP_WAKEUP_EXT0:
        /* Button event */
        log_v("Button: Interrupt");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        /* LoRa event */
        log_v("LoRa: Interrupt");
        uint8_t irq_flags;
        SPI.beginTransaction(SPISettings());
        SPI.transfer(RADIOLIB_SX127X_REG_IRQ_FLAGS);
        irq_flags = SPI.transfer(0x00);
        SPI.endTransaction();
        log_d("LoRa: IRQ flags 0x%02x", irq_flags);
        vTaskResume(task_lora_dio0_handler);
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        switch (loop_state)
        {
        case STATE_SEND:
        case STATE_RECEIVE:
            /* LoRa operation timeout */
            log_v("Timer: LoRa operation timeout");

            display.setFont(&FreeMono9pt7b);
            display.setCursor(0, 48);
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
            display.write("timeout...");
            display.display();

            /* Go to sleep */
            loop_state = STATE_DISPLAY;
            goToSleep(TIMER_DISPLAY_DURATION_MS, false, true);
            break;
        default:
            /* Display and other unsupported cases - simple switch all off */
            log_v("Timer: Display timeout");

            /* Go to sleep */
            loop_state = STATE_IDLE;
            digitalWrite(VEXT_CONTROL_PIN, HIGH);
            goToSleep(0, false, false);
            break;
        }
        break;
    }
}

void loop() {
    /* Do nothing */
}
