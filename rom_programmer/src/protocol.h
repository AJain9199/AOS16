#ifndef PROTOCOL_H
#define PROTOCOL_H

// ---------------------------------------------------------------------------
// Host → Arduino commands (single byte)
// ---------------------------------------------------------------------------
#define CMD_PING          'P'   // Ping — check connection
#define CMD_GET_ID        'I'   // Read manufacturer + device ID
#define CMD_CHIP_ERASE    'E'   // Erase entire chip
#define CMD_SECTOR_ERASE  'S'   // Erase one 4 KB sector
                                //   payload: addr[2:0] (3 bytes, big-endian)
#define CMD_WRITE         'W'   // Write N bytes starting at address
                                //   payload: addr[2:0], len[1 byte], data[len]
                                //   len = 0 means 256 bytes
#define CMD_READ          'R'   // Read N bytes starting at address
                                //   payload: addr[2:0], len[1:0] (2 bytes BE)

// ---------------------------------------------------------------------------
// Arduino → Host responses
// ---------------------------------------------------------------------------
#define RESP_OK     'K'
#define RESP_ERR    'F'
#define RESP_READY  'Z'   // Sent once on startup so the host knows Arduino is up

// ---------------------------------------------------------------------------
// Dispatch incoming serial commands.  Call from loop().
// ---------------------------------------------------------------------------
void protocol_handle();

#endif //PROTOCOL_H
