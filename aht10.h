#ifndef AHT10_H
#define AHT10_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define AHT10_ADDR 0x38

// Struct to hold temperature and humidity data
typedef struct {
    float temperature; // Temperature in °C
    float humidity;    // Humidity relative in %
} aht10_data_t;

// Public functions
bool aht10_init(i2c_inst_t* i2c_port);
bool aht10_read_data(i2c_inst_t* i2c_port, aht10_data_t* dado);

#endif