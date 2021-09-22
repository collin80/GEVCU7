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

#pragma once

#include "serial_io.h"

#define ESP32_BOOT      6
#define ESP32_ENABLE    45

/**
  * @brief Initializes serial interface.
  *
  * @param baud_rate[in]       Communication speed.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_FAIL Initialization failure
  */
esp_loader_error_t loader_port_gevcu_init(uint32_t baud_rate);

/**
  * @brief Deinitialize serial interface.
  */
void loader_port_gevcu_deinit(void);
