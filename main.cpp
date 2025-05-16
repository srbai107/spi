#include "hardware/spi.h"
#include "pico/stdlib.h"
#include <stdbool.h>
#include <string.h>

// === NRF24L01+ Commands and Registers ===
#define R_REGISTER    0x00
#define W_REGISTER    0x20
#define R_RX_PAYLOAD  0x61
#define W_TX_PAYLOAD  0xA0
#define FLUSH_TX      0xE1
#define FLUSH_RX      0xE2
#define NOP           0xFF

#define CONFIG        0x00
#define EN_AA         0x01
#define EN_RXADDR     0x02
#define SETUP_AW      0x03
#define SETUP_RETR    0x04
#define RF_CH         0x05
#define RF_SETUP      0x06
#define STATUS        0x07
#define RX_ADDR_P0    0x0A
#define TX_ADDR       0x10
#define RX_PW_P0      0x11
#define FIFO_STATUS   0x17

typedef struct {
    spi_inst_t *spi;
    uint csn_pin;
    uint ce_pin;
} nrf24_t;

// ======= Internal Helpers =======
static void csn(nrf24_t *nrf, bool level) { gpio_put(nrf->csn_pin, level); }
static void ce(nrf24_t *nrf, bool level)  { gpio_put(nrf->ce_pin, level); }

static void nrf_write_reg(nrf24_t *nrf, uint8_t reg, const uint8_t *data, size_t len) {
    csn(nrf, false);
    uint8_t cmd = W_REGISTER | (reg & 0x1F);
    spi_write_blocking(nrf->spi, &cmd, 1);
    spi_write_blocking(nrf->spi, data, len);
    csn(nrf, true);
}

static void nrf_read_reg(nrf24_t *nrf, uint8_t reg, uint8_t *data, size_t len) {
    csn(nrf, false);
    uint8_t cmd = R_REGISTER | (reg & 0x1F);
    spi_write_blocking(nrf->spi, &cmd, 1);
    spi_read_blocking(nrf->spi, 0xFF, data, len);
    csn(nrf, true);
}

// ======= Public Functions =======

void nrf24_init(nrf24_t *nrf, spi_inst_t *spi, uint csn_pin, uint ce_pin, bool is_rx) {
    nrf->spi = spi;
    nrf->csn_pin = csn_pin;
    nrf->ce_pin = ce_pin;

    gpio_init(csn_pin); gpio_set_dir(csn_pin, GPIO_OUT); csn(nrf, true);
    gpio_init(ce_pin);  gpio_set_dir(ce_pin, GPIO_OUT);  ce(nrf, false);

    sleep_ms(100);

    uint8_t val;

    val = 0x0E; nrf_write_reg(nrf, CONFIG, &val, 1);
    val = 0x01; nrf_write_reg(nrf, EN_AA, &val, 1);
    val = 0x01; nrf_write_reg(nrf, EN_RXADDR, &val, 1);
    val = 0x03; nrf_write_reg(nrf, SETUP_AW, &val, 1);
    val = 0x4F; nrf_write_reg(nrf, SETUP_RETR, &val, 1);
    val = 76;   nrf_write_reg(nrf, RF_CH, &val, 1);
    val = 0x06; nrf_write_reg(nrf, RF_SETUP, &val, 1);
    val = 32;   nrf_write_reg(nrf, RX_PW_P0, &val, 1);

    uint8_t addr[5] = { 'n', 'R', 'F', '2', '4' };
    nrf_write_reg(nrf, RX_ADDR_P0, addr, 5);
    nrf_write_reg(nrf, TX_ADDR, addr, 5);

    if (is_rx) ce(nrf, true);
}

void nrf24_send(nrf24_t *nrf, const uint8_t *data, size_t len) {
    ce(nrf, false);
    csn(nrf, false);
    uint8_t cmd = W_TX_PAYLOAD;
    spi_write_blocking(nrf->spi, &cmd, 1);
    spi_write_blocking(nrf->spi, data, len);
    csn(nrf, true);
    ce(nrf, true);
    sleep_ms(1);
    ce(nrf, false);
}

bool nrf24_data_ready(nrf24_t *nrf) {
    uint8_t status;
    nrf_read_reg(nrf, STATUS, &status, 1);
    return status & 0x40;
}

void nrf24_read(nrf24_t *nrf, uint8_t *data, size_t len) {
    csn(nrf, false);
    uint8_t cmd = R_RX_PAYLOAD;
    spi_write_blocking(nrf->spi, &cmd, 1);
    spi_read_blocking(nrf->spi, 0xFF, data, len);
    csn(nrf, true);

    uint8_t clear = 0x40;
    nrf_write_reg(nrf, STATUS, &clear, 1);
}
