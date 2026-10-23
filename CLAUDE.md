# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

GEVCU7 is firmware for the Generalized Electric Vehicle Control Unit, running on a Teensy MicroMod (i.MX RT1062) processor. It interfaces with electric vehicle hardware (motor controllers, BMS, chargers, DC-DC converters, etc.) primarily over CAN bus. This is an embedded Arduino-framework project that **only compiles with PlatformIO** — the Arduino IDE cannot handle its directory layout.

## Build Commands

```bash
# Build (from project root)
pio run

# Build and upload to connected device
pio run --target upload

# Clean build artifacts
pio run --target clean

# Run host-side unit tests (see test/README)
pio test -e native
```

The project targets the `teensymm` environment (Teensy MicroMod). The `platformio.ini` pulls TeensyDuino 1.60 beta packages from a specific GitHub branch. Extra libraries come from `~/Arduino/libraries`.

Upload scripts for special builds:
- `upload.sh` — standard firmware upload
- `upload_hr.sh` — HyperRocket variant upload

## Architecture

### Core Execution Model

The main loop (`src/GEVCU7.cpp`) is minimal. All real work happens through two mechanisms:

1. **TickHandler** — periodic timers (backed by `TeensyTimerTool`). Devices register with `tickHandler.attach(this, intervalMicros)` and receive `handleTick()` calls. With `CFG_TIMER_USE_QUEUING` defined (it is), ticks are queued and dispatched from the main loop rather than running directly in interrupt context.

2. **CanHandler** — three CAN bus instances (`canHandlerBus0/1/2`). Devices implement `CanObserver` and call `handler.attach(observer, id, mask, extended)` to receive `handleCanFrame()` callbacks. `canHandlerBus1` is the isolated bus alias (`canHandlerIsolated`), `canHandlerBus2` is CAN-FD (`canHandlerFD`).

The main loop calls `canEvents()` each iteration to drain incoming CAN frames and dispatch them.

### Device System

Every peripheral is a `Device` subclass (in `src/devices/`). The lifecycle is:

- **Constructor** — runs unconditionally at global init time; registers with `DeviceManager`. Set `commonName`, `shortName`, initialize variables.
- **`earlyInit()`** — called for all devices whether enabled or not; create the `PrefHandler` here.
- **`setup()`** — called only if the device is enabled; do hardware init, load config, register CAN observers and tick handlers, populate `cfgEntries`.
- **`handleTick()`** — periodic work.
- **`handleCanFrame()`** — incoming CAN message handling (for `CanObserver` subclasses).
- **`loadConfiguration()` / `saveConfiguration()`** — EEPROM persistence via `PrefHandler`.

To register a new device, simply instantiate it as a global at the bottom of the `.cpp` file (e.g., `Example example;`). The constructor calls `deviceManager.addDevice(this)`. No central registration list exists.

### Configuration & EEPROM (`PrefHandler`)

Settings are stored in a 256 KB external EEPROM chip. `PrefHandler` uses FNV hashes of string key names to locate entries — there is no fixed address map per device. Each device gets a 1 KB block starting at `EE_DEVICES_BASE` (address 1024).

Devices expose user-editable settings via `cfgEntries` (vector of `ConfigEntry`). Each entry holds a pointer directly into the device's configuration struct, the variable type, min/max bounds, an optional describe-function, and an optional post-update callback. The serial console and web interface enumerate these automatically.

### Device Hierarchy

```
Device (base, TickObserver)
├── MotorController  →  DmocMotorController, LeafMotorController, RMSMotorController, SevconMotorController, etc.
├── Throttle         →  PotThrottle, CanThrottle, TestThrottle
├── BatteryManager   →  OrionBatteryManager, ThinkBatteryManager, SimpleBatteryManager
├── ChargeController →  TCCHCharger
├── DCDCController   →  DelphiDCDC, OvarDCDC, TeslaSDCDC
├── HVACDevice       →  TeslaAC_Gen2
└── misc             →  Precharger, PowerController, LightController, SystemDevice, etc.
```

Device type IDs are defined in each device's own header (not a central enum). System-reserved IDs are 0x7000–0x7FFF. Bit 15 (0x8000) is the enable/disable flag in the device table stored in EEPROM.

### Key Singletons

| Global | Header | Purpose |
|--------|--------|---------|
| `deviceManager` | `DeviceManager.h` | Central registry; send inter-device messages |
| `tickHandler` | `TickHandler.h` | Timer multiplexer |
| `canHandlerBus0/1/2` | `CanHandler.h` | CAN bus I/O |
| `systemIO` | `sys_io.h` | ADC, digital I/O, PWM outputs |
| `faultHandler` | `FaultHandler.h` | Fault logging to EEPROM |
| `crashHandler` | `CrashHandler.h` | Post-crash breadcrumb capture |
| `memCache` | `MemCache.h` | Write-back cache for EEPROM I²C chip |

### CANOpen Support

`CanHandler` has built-in CANOpen support (NMT, PDO, SDO). `CanObserver` subclasses can call `setCANOpenMode(true)` and receive `handlePDOFrame()`, `handleSDORequest()`, `handleSDOResponse()` callbacks instead of raw `handleCanFrame()`.

### OTA Firmware Update

`FlasherX` / `FlashTxx` implement in-application flashing for the Teensy. At boot, if `GEVCU7.hex` exists on the SD card it is flashed automatically. The ESP32 co-processor (handles WiFi/BT) is flashed similarly from `esp32_*.bin` files via `ESP32Driver` / `esp_loader`.

### Serial Console

`SerialConsole` provides a runtime menu over USB serial. Commands follow the pattern `SETTINGNAME=value`. The console enumerates all registered `ConfigEntry` items from all devices, so new settings become accessible automatically once added to `cfgEntries`.

## Adding a New Device Driver

Use `docs/ExampleDevice.h` and `docs/ExampleDevice.cpp` as a template. Key points:
1. Choose a unique 16-bit device ID (hex, under 0x8000).
2. Inherit from the appropriate base class (`Device`, `MotorController`, `Throttle`, etc.).
3. Inherit from `CanObserver` as well if you need CAN.
4. In `earlyInit()`, create `prefsHandler = new PrefHandler(YOUR_ID)`.
5. In `setup()`, call `tickHandler.attach(this, interval)` and `canHandlerBusN.attach(this, id, mask, extended)`.
6. Instantiate a global instance at the bottom of the `.cpp` — this self-registers.

## localconfig.h

`src/localconfig.h` is intended for local-only tweaks that shouldn't be committed. Key defines:
- `DEBUG_STARTUP_DELAY` — adds a startup delay to see early serial output. **Disable for production builds.**
- `ASSUME_SDCARD_INSERTED` — bypasses SD card detect pin; needed on first prototype hardware.

## Hardware Notes

- **Target MCU**: Teensy MicroMod (i.MX RT1062, Cortex-M7 @ 600 MHz)
- **CAN buses**: 3 buses — Bus 0 (standard), Bus 1 (isolated), Bus 2 (CAN-FD capable)
- **I/O**: 8 analog inputs, 12 digital inputs, 8 digital outputs, 4 PWM outputs, 24 extended I/O via PCA I²C expander
- **Watchdog**: `WDT_T4<WDT3>` (RTWDT), 5-second timeout, must call `wdt.feed()` in the loop
- **System variants**: `GEVCU7A` through `GEVCU7D` (enum in `sys_io.h`)
