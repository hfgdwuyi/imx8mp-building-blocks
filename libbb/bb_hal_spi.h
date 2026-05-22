#ifndef BB_HAL_SPI_H
#define BB_HAL_SPI_H
#include <stdint.h>
#include <stddef.h>

// SPI device abstraction over /dev/spidevX.Y
typedef struct {
    int     fd;
    char    device[64];
    uint32_t speed_hz;
    uint8_t  mode;       // SPI_MODE_0..3
    uint8_t  bits_per_word;
} bb_spi_t;

// SPI modes
#define BB_SPI_MODE_0  (0)
#define BB_SPI_MODE_1  (1)
#define BB_SPI_MODE_2  (2)
#define BB_SPI_MODE_3  (3)
#define BB_SPI_CPHA    0x01
#define BB_SPI_CPOL    0x02

// Open SPI device (e.g. "/dev/spidev1.0")
int  bb_spi_open(bb_spi_t *spi, const char *device, uint32_t speed_hz, uint8_t mode, uint8_t bits);

// Full-duplex transfer: send tx_buf, receive into rx_buf (can be same buffer for half-duplex)
int  bb_spi_transfer(bb_spi_t *spi, const uint8_t *tx, uint8_t *rx, size_t len);

// Write only (rx data discarded)
int  bb_spi_write(bb_spi_t *spi, const uint8_t *data, size_t len);

// Close
void bb_spi_close(bb_spi_t *spi);

#endif
