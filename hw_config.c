
// Bibliotecas padrão
#include <assert.h>     // For runtime assertion checks
#include <string.h>     // For string manipulation

// Bibliotecas do projeto
#include "lib\FatFs_SPI\include\my_debug.h"   // Custom library for debugging
#include "lib\FatFs_SPI\sd_driver\hw_config.h"  // Project-specific hardware configuration

// Bibliotecas do sistema de arquivos FAT
#include "lib\FatFs_SPI\ff15\source\ff.h"         // Integer types and functions of the FAT file system
#include "lib\FatFs_SPI\ff15\source\diskio.h"     // Disk access function declarations

/* 
Assumed hardware configuration for SPI communication with MicroSD card:

|       | SPI0  | GPIO  | Pin   | SPI       | MicroSD   |       Description       | 
| ----- | ----  | ----- | ---   | --------  | --------- | ----------------------- |
| MISO  | RX    | 16    | 21    | DO        | DO        | Card data for MCU       |
| MOSI  | TX    | 19    | 25    | DI        | DI        | MCU data for card       |
| SCK   | SCK   | 18    | 24    | SCLK      | CLK       | SPI bus clock           |
| CS0   | CSn   | 17    | 22    | SS ou CS  | CS        | SD card selection       |
| DET   |       | 22    | 29    |           | CD        | Card detection          |
| GND   |       |       | 18,23 |           | GND       | Ground                  |
| 3v3   |       |       | 36    |           | 3v3       | 3.3V power supply       |
*/

// ========================== SPI Configuration ==========================

// Configuration array for SPI interfaces
static spi_t spis[] = {
    {
        .hw_inst = spi0,      // SPI hardware instance used
        .miso_gpio = 16,      // GPIO for MISO (Data Input)
        .mosi_gpio = 19,      // GPIO to MOSI (Data Output)
        .sck_gpio = 18,       // GPIO to SPI clock
        .baud_rate = 1000000  // Transmission rate: 1 Mbps
        // Commented alternative: 25 Mbps (actual frequency: ~20.8 MHz)
    }
};

// ========================== SD card configuration ==========================

// SD Card Settings Array
static sd_card_t sd_cards[] = {
    {
        .pcName = "0:",             // Logical device name (used for mounting)
        .spi = &spis[0],            // Pointer to the corresponding SPI interface
        .ss_gpio = 17,              // GPIO for card selection (CS)
        .use_card_detect = false,   // Disables card presence verification
        .card_detect_gpio = 22,     // GPIO that could be used to detect the card
        .card_detected_true = -1    // Expected value to indicate card presence
    }
};

// ========================== Access roles ==========================

// Returns the number of configured SD cards
size_t sd_get_num() {
    return count_of(sd_cards);
}

// Returns the pointer to the SD card index 'num'
sd_card_t *sd_get_by_num(size_t num) {
    assert(num <= sd_get_num()); // Ensures that the index is valid
    if (num <= sd_get_num()) {
        return &sd_cards[num];   // Returns pointer to card
    } else {
        return NULL;             // Returns NULL if invalid index
    }
}

// Returns the number of configured SPI interfaces
size_t spi_get_num() {
    return count_of(spis);
}

// Returns the pointer to the SPI interface at index 'num'
spi_t *spi_get_by_num(size_t num) {
    assert(num <= spi_get_num()); // Ensures that the index is valid
    if (num <= spi_get_num()) {
        return &spis[num];        // Returns pointer to SPI interface
    } else {
        return NULL;              // Returns NULL if invalid index
    }
}

