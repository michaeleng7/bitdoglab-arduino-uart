#include "aht10.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"


// Includes the FatFs library
#include "lib/FatFs_SPI/ff15/source/ff.h"

// Defines the UART port and baud rate settings
#define UART_ID uart0
#define BAUD_RATE 9600
#define UART_TX_PIN 0
#define UART_RX_PIN 1

// Define the RGB LED pins
#define LED_RED_PIN 13
#define LED_GREEN_PIN 11
#define LED_BLUE_PIN 12

// Set the buzzer pin
#define BUZZER_PIN 21

// Define the I2C pins for the AHT10 sensor
#define I2C1_PORT i2c1
#define I2C1_SDA_PIN 2
#define I2C1_SCL_PIN 3

// Authorized UIDs.
const char *AUTHORIZED_UIDS[] = {
    "224c8d04",
    "b4067e05",
};
const int NUM_AUTHORIZED_UIDS = 2;

// Variables for the file system
FATFS fs;
FIL fil;

// Global variables for sensor data
aht10_data_t sensor_data;

// Variable to control whether sensor logging is enabled
bool sensor_logging_enabled = false;

// Function to control the RGB LED
void set_rgb_color(int r, int g, int b) {
    gpio_put(LED_RED_PIN, r);
    gpio_put(LED_GREEN_PIN, g);
    gpio_put(LED_BLUE_PIN, b);
}

// Buzzer function
void buzzer_tone(uint32_t duration_ms) {
    gpio_put(BUZZER_PIN, 1);
    sleep_ms(duration_ms);
    gpio_put(BUZZER_PIN, 0);
}

// Initialize the SD card and mount the file system
void initialize_sd() {
    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        printf("Failed to mount SD card: %d\n", fr);
    } else {
        printf("SD card mounted successfully.\n");
    }
}

// Function to record data in the log file
void log_event(const char* event_type, const char* message) {
    FRESULT fr = f_open(&fil, "log.txt", FA_OPEN_APPEND | FA_WRITE);
    if (fr == FR_OK) {
        uint64_t current_time_ms = to_ms_since_boot(get_absolute_time());
        unsigned long minutes = current_time_ms / 60000;
        unsigned long seconds = (current_time_ms / 1000) % 60;

        f_printf(&fil, "[%02lu:%02lu] %s: %s\n", 
                 minutes, seconds, event_type, message);
        f_close(&fil);
    } else {
        printf("Failed to open file for writing: %d\n", fr);
    }
}

// Function to log sensor data
void log_sensor_data(float temp, float hum) {
    char data_str[50];
    snprintf(data_str, sizeof(data_str), "Temp=%.2f C, Hum=%.2f %%", temp, hum);
    log_event("SENSOR_READ", data_str);
}

// Function to register access events
void log_access_event(const char* uid, const char* status) {
    char access_str[50];
    snprintf(access_str, sizeof(access_str), "UID=%s, Status=%s", uid, status);
    log_event("ACCESS_EVENT", access_str);
}

int main() {
    stdio_init_all();

    // Initialize the UART
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Configure the LED and buzzer pins
    gpio_init(LED_RED_PIN);
    gpio_init(LED_GREEN_PIN);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);

    // Initialize I2C1 for the AHT10 sensor
    i2c_init(I2C1_PORT, 100 * 1000);
    gpio_set_function(I2C1_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA_PIN);
    gpio_pull_up(I2C1_SCL_PIN);

    // Initialize the SD card
    initialize_sd();

    // Initialize the AHT10 sensor
    if (aht10_init(I2C1_PORT)) {
        printf("AHT10 sensor initialized successfully!\n");
    } else {
        printf("AHT10 sensor initialization failed!\n");
    }

    printf("BitDogLab: Waiting for Arduino UID...\n");
    
    char buffer[50];
    int idx = 0;

    uint64_t last_aht10_read_time = to_ms_since_boot(get_absolute_time());
    const uint32_t AHT10_READ_INTERVAL_MS = 60000; // 60 seconds

    while (1) {
        // Periodic reading of the AHT10 sensor every minute, if enabled
        if (sensor_logging_enabled) {
            uint64_t current_time_ms = to_ms_since_boot(get_absolute_time());
            if (current_time_ms - last_aht10_read_time >= AHT10_READ_INTERVAL_MS) {
                if (aht10_read_data(I2C1_PORT, &sensor_data)) {
                    printf("[LOGGING] Temp=%.2f C, Hum=%.2f %%\n", sensor_data.temperature, sensor_data.humidity);
                    log_sensor_data(sensor_data.temperature, sensor_data.humidity);
                } else {
                    printf("Failed to read AHT10 data.\n");
                }
                last_aht10_read_time = current_time_ms;
            }
        }
        
        // Check data from UART (RFID tag)
        if (uart_is_readable(UART_ID)) {
            char c = uart_getc(UART_ID);
            
            if (c == '\n' || c == '\r') {
                buffer[idx] = '\0';
                
                if (idx > 0 && strcmp(buffer, "Arduino: Ready to read tags!") != 0) {
                    printf("Received UID: %s\n", buffer);

                    int access_granted = 0;
                    for (int i = 0; i < NUM_AUTHORIZED_UIDS; i++) {
                        if (strcmp(buffer, AUTHORIZED_UIDS[i]) == 0) {
                            access_granted = 1;
                            break;
                        }
                    }
                    
                    if (access_granted) {
                        printf("Access Released!\n");
                        log_access_event(buffer, "RELEASED"); // Save the event in the log and print it on the serial
                        set_rgb_color(0, 1, 0); // Green
                        buzzer_tone(250);
                        sleep_ms(2000);
                        set_rgb_color(0, 0, 0); // Turn off the LEDs
                        sensor_logging_enabled = true; // Enables sensor logging
                    } else {
                        printf("Access Denied!\n");
                        log_access_event(buffer, "DENIED"); // Saves the event to the log and prints it to the serial
                        set_rgb_color(1, 0, 0); // Red
                        buzzer_tone(500);
                        sleep_ms(2000);
                        set_rgb_color(0, 0, 0); // Turn off the LEDs
                        sensor_logging_enabled = false; // Disables sensor logging
                    }
                }
                idx = 0;
            } else {
                if (idx < 49) {
                    buffer[idx++] = c;
                }
            }
        }
    }

    return 0;
}