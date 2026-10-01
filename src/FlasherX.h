#pragma once

#include <Arduino.h>
#include "SD.h"
#include "intel_hex.h"

void update_firmware( FsFile *file, uint32_t buffer_addr, uint32_t buffer_size );
void setup_flasherx();
void start_upgrade(FsFile *file);