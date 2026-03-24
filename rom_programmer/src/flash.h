//
// Created by Aryam on 24-03-2026.
//

#ifndef FLASH_H
#define FLASH_H

#pragma once
#include <Arduino.h>

// Sector size — uniform 4 KB across all three variants (datasheet §3.3)
#define FLASH_SECTOR_SIZE       0x1000UL

// Per-device chip sizes
#define FLASH_SIZE_SST39SF010A  0x020000UL   // 128 KB
#define FLASH_SIZE_SST39SF020A  0x040000UL   // 256 KB
#define FLASH_SIZE_SST39SF040   0x080000UL   // 512 KB

// ---------------------------------------------------------------------------
// Initialise GPIO — call once from setup() before any flash operation.
// ---------------------------------------------------------------------------
void flash_init();

// ---------------------------------------------------------------------------
// Read a single byte.
// ---------------------------------------------------------------------------
uint8_t flash_read(uint32_t address);

// ---------------------------------------------------------------------------
// Program a single byte using the JEDEC SDP command sequence.
// Writing 0xFF is skipped (chip erases to 0xFF, no program needed).
// Returns true on success, false on timeout.
// ---------------------------------------------------------------------------
bool flash_write_byte(uint32_t address, uint8_t data);

// ---------------------------------------------------------------------------
// Erase the 4 KB sector that contains 'address'.
// Returns true on success, false on timeout (>200 ms).
// ---------------------------------------------------------------------------
bool flash_sector_erase(uint32_t address);

// ---------------------------------------------------------------------------
// Erase the entire chip.
// Returns true on success, false on timeout (>300 ms).
// ---------------------------------------------------------------------------
bool flash_chip_erase();

// ---------------------------------------------------------------------------
// Read manufacturer and device IDs via the Software Product ID sequence.
// Exits ID mode before returning.
//   SST39SF010A: mfr=0xBF  dev=0xB5
//   SST39SF020A: mfr=0xBF  dev=0xB6
//   SST39SF040:  mfr=0xBF  dev=0xB7
// ---------------------------------------------------------------------------
void flash_get_id(uint8_t &mfr_id, uint8_t &dev_id);

#endif //FLASH_H
