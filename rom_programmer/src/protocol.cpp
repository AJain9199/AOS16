#include "protocol.h"
#include "flash.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Internal helpers — block until the requested bytes arrive
// ---------------------------------------------------------------------------

static uint8_t g_buf[256];   // shared write/read staging buffer

static uint8_t recv_byte() {
    while (!Serial.available()) {}
    return static_cast<uint8_t>(Serial.read());
}

// Read a 3-byte big-endian address
static uint32_t recv_addr() {
    uint32_t a  = static_cast<uint32_t>(recv_byte()) << 16;
    a |= static_cast<uint32_t>(recv_byte()) << 8;
    a |= recv_byte();
    return a;
}

// Read a 3-byte big-endian length
static uint32_t recv_len24() { return recv_addr(); }

// Read a 2-byte big-endian unsigned length
static uint16_t recv_len16() {
    uint16_t n  = static_cast<uint16_t>(recv_byte()) << 8;
    n |= recv_byte();
    return n;
}

// ---------------------------------------------------------------------------
// Command dispatcher
// ---------------------------------------------------------------------------

void protocol_handle() {
    if (!Serial.available()) return;
    const uint8_t cmd = recv_byte();

    switch (cmd) {

        // ----------------------------------------------------------------
        // P — Ping
        // ----------------------------------------------------------------
        case CMD_PING:
            Serial.write(RESP_OK);
            break;

        // ----------------------------------------------------------------
        // I — Get IDs
        // Response: RESP_OK, mfr_id, dev_id
        // ----------------------------------------------------------------
        case CMD_GET_ID: {
            uint8_t mfr = 0, dev = 0;
            flash_get_id(mfr, dev);
            Serial.write(RESP_OK);
            Serial.write(mfr);
            Serial.write(dev);
            break;
        }

        // ----------------------------------------------------------------
        // E — Chip erase  (no payload)
        // Response: RESP_OK | RESP_ERR
        // ----------------------------------------------------------------
        case CMD_CHIP_ERASE:
            Serial.write(flash_chip_erase() ? RESP_OK : RESP_ERR);
            break;

        // ----------------------------------------------------------------
        // S — Sector erase
        // Payload:  addr[2:0]  (3 bytes, big-endian)
        // Response: RESP_OK | RESP_ERR
        // ----------------------------------------------------------------
        case CMD_SECTOR_ERASE: {
            const uint32_t addr = recv_addr();
            Serial.write(flash_sector_erase(addr) ? RESP_OK : RESP_ERR);
            break;
        }

        // ----------------------------------------------------------------
        // W — Write bytes
        // Payload:  addr[2:0], len (1 byte, 0 = 256), data[len]
        // Response: RESP_OK | RESP_ERR
        //
        // Bytes are read from serial and programmed one at a time so the
        // 64-byte hardware receive buffer is never overwhelmed regardless
        // of chunk size.
        // ----------------------------------------------------------------
        case CMD_WRITE: {
            const uint32_t addr = recv_addr();
            const uint8_t  raw_len = recv_byte();
            // len=0 encodes 256 so we fit in one byte while still allowing
            // a full 256-byte write in a single command.
            const uint16_t len = (raw_len == 0) ? 256 : raw_len;
            // Drain the whole payload into RAM first, then program it in one
            // tight burst.  Programming (~30 us/byte) is slower than the
            // serial link, so the host may already be sending the next chunk
            // into the RX ring buffer while this one is being programmed.
            for (uint16_t i = 0; i < len; i++) g_buf[i] = recv_byte();
            Serial.write(flash_write_block(addr, g_buf, len) ? RESP_OK : RESP_ERR);
            break;
        }

        // ----------------------------------------------------------------
        // R — Read bytes
        // Payload:  addr[2:0], len[1:0]  (2-byte big-endian length)
        // Response: data[len], RESP_OK
        // ----------------------------------------------------------------
        case CMD_READ: {
            const uint32_t addr = recv_addr();
            const uint16_t len  = recv_len16();
            for (uint16_t done = 0; done < len; ) {
                const uint16_t rem = len - done;
                const uint16_t n   = rem < 64 ? rem : 64;
                flash_read_block(addr + done, g_buf, n);
                Serial.write(g_buf, n);
                done += n;
            }
            Serial.write(RESP_OK);
            break;
        }

        // ----------------------------------------------------------------
        // C — CRC-16/XMODEM of a range, computed on the device
        // Payload:  addr[2:0], len[2:0]  (3-byte big-endian each)
        // Response: crc_hi, crc_lo, RESP_OK
        // ----------------------------------------------------------------
        case CMD_CRC: {
            const uint32_t addr = recv_addr();
            const uint32_t len  = recv_len24();
            const uint16_t crc  = flash_crc16(addr, len);
            Serial.write(static_cast<uint8_t>(crc >> 8));
            Serial.write(static_cast<uint8_t>(crc));
            Serial.write(RESP_OK);
            break;
        }

        // ----------------------------------------------------------------
        // Unknown command
        // ----------------------------------------------------------------
        default:
            Serial.write(RESP_ERR);
            break;
    }
}