#ifndef BB_HAL_I2C_H
#define BB_HAL_I2C_H
#include <stdint.h>
#include <stddef.h>

// I2C device abstraction over /dev/i2c-X
typedef struct {
    int     fd;
    char    device[64];
    uint8_t addr;      // current slave address (7-bit)
} bb_i2c_t;

// Open I2C bus (e.g. "/dev/i2c-1")
int  bb_i2c_open(bb_i2c_t *i2c, const char *device);

// Set target slave address (7-bit)
int  bb_i2c_set_addr(bb_i2c_t *i2c, uint8_t addr);

// Write bytes to slave (no register address)
int  bb_i2c_write(bb_i2c_t *i2c, uint8_t addr, const uint8_t *data, size_t len);

// Read bytes from slave (no register address)
int  bb_i2c_read(bb_i2c_t *i2c, uint8_t addr, uint8_t *buf, size_t len);

// Combined: write register address then read data (SMBus-style)
int  bb_i2c_write_read(bb_i2c_t *i2c, uint8_t addr,
                       const uint8_t *wbuf, size_t wlen,
                       uint8_t *rbuf, size_t rlen);

// Quick probe: send address and check ACK. Returns 0 if device present.
int  bb_i2c_probe(bb_i2c_t *i2c, uint8_t addr);

// Close
void bb_i2c_close(bb_i2c_t *i2c);

#endif
