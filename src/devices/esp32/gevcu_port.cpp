/* Copyright 2020 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Arduino.h"
#include "gevcu_port.h"

// #define SERIAL_DEBUG_ENABLE

#ifdef SERIAL_DEBUG_ENABLE

static void dec_to_hex_str(const uint8_t dec, uint8_t hex_str[3])
{
    static const uint8_t dec_to_hex[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };

    hex_str[0] = dec_to_hex[dec >> 4];
    hex_str[1] = dec_to_hex[dec & 0xF];
    hex_str[2] = '\0';
}

static void serial_debug_print(const uint8_t *data, uint16_t size, bool write)
{
    static bool write_prev = false;
    uint8_t hex_str[3];

    if(write_prev != write) {
        write_prev = write;
        printf("\n--- %s ---\n", write ? "WRITE" : "READ");
    }

    for(uint32_t i = 0; i < size; i++) {
        dec_to_hex_str(data[i], hex_str);
        printf("%s ", hex_str);
    }
}

#else

static void serial_debug_print(const uint8_t *data, uint16_t size, bool write) { }

#endif

static uint32_t s_time_end;

esp_loader_error_t loader_port_gevcu_init(uint32_t baud_rate)
{
    Serial2.begin(baud_rate);

    pinMode(ESP32_BOOT, OUTPUT);
    pinMode(ESP32_ENABLE, OUTPUT);

    return ESP_LOADER_SUCCESS;
}

void loader_port_gevcu_deinit(void)
{
    //Could call Serial2.end but I don't really want to ever stop the serial port.
    //uart_driver_delete(s_uart_port);
}


esp_loader_error_t loader_port_serial_write(const uint8_t *data, uint16_t size, uint32_t timeout)
{
    serial_debug_print(data, size, true);

    Serial2.write((const char *)data, size);
    Serial2.flush();

    return ESP_LOADER_SUCCESS;
}


esp_loader_error_t loader_port_serial_read(uint8_t *data, uint16_t size, uint32_t timeout)
{
    Serial2.setTimeout(timeout);
    int read = Serial2.readBytes(data, size);

    serial_debug_print(data, read, false);

    if (read < 0) {
        return ESP_LOADER_ERROR_FAIL;
    } else if (read < size) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_SUCCESS;
    }
}


// Set GPIO0 LOW, then
// assert reset pin for 50 milliseconds.
void loader_port_enter_bootloader(void)
{
    digitalWrite(ESP32_BOOT, LOW);
    loader_port_reset_target();
    loader_port_delay_ms(50);
    digitalWrite(ESP32_BOOT, HIGH);
}


void loader_port_reset_target(void)
{
    digitalWrite(ESP32_ENABLE, LOW);
    loader_port_delay_ms(50);
    digitalWrite(ESP32_ENABLE, HIGH);
}


void loader_port_delay_ms(uint32_t ms)
{
    delay(ms);
}


void loader_port_start_timer(uint32_t ms)
{
    s_time_end = millis() + ms * 1000;
}


uint32_t loader_port_remaining_time(void)
{
    int64_t remaining = (s_time_end - millis()) / 1000;
    return (remaining > 0) ? (uint32_t)remaining : 0;
}


void loader_port_debug_print(const char *str)
{
    Serial2.printf("DEBUG: %s\n", str);
}

esp_loader_error_t loader_port_change_baudrate(uint32_t baudrate)
{
    Serial2.begin(baudrate);

    return ESP_LOADER_SUCCESS;
}
