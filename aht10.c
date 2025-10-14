#include "aht10.h"

// Comands of the AHT10 sensor
const uint8_t CMD_INIT[] = {0xE1, 0x08, 0x00};
const uint8_t CMD_MEASURE[] = {0xAC, 0x33, 0x00};

bool aht10_init(i2c_inst_t* i2c) {
    // Send initialization command
    int ret = i2c_write_blocking(i2c, AHT10_ADDR, CMD_INIT, sizeof(CMD_INIT), false);
    if (ret < 0) return false;
    sleep_ms(20); // Wait for the sensor to initialize
    return true;
}

bool aht10_read_data(i2c_inst_t* i2c, aht10_data_t* dado) {
    // 1. Send measurement command
    int ret = i2c_write_blocking(i2c, AHT10_ADDR, CMD_MEASURE, sizeof(CMD_MEASURE), false);
    if (ret < 0) return false;

    // 2. Wait for the measurement to complete (typically 75ms)
    sleep_ms(80);

    // 3. Read 6 bytes of data
    uint8_t buf[6];
    ret = i2c_read_blocking(i2c, AHT10_ADDR, buf, sizeof(buf), false);
    if (ret < 0) return false;

    // 4. Check the status byte to see if the sensor is calibrated and not busy
    // The bit 7 of the first byte indicates if the sensor is busy (0 = busy, 1 = ready)
    if ((buf[0] & 0x88) != 0x08) {
        return false;
    }

    // 5. Calculate humidity and temperature values based on the datasheet formulas
    uint32_t raw_umidade = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | (buf[3] >> 4);
    dado->humidity = ((float)raw_umidade / 1048576.0f) * 100.0f;

    uint32_t raw_temp = (((uint32_t)buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | buf[5];
    dado->temperature = (((float)raw_temp / 1048576.0f) * 200.0f) - 50.0f;

    return true;
}