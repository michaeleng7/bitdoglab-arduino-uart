/*******************************************************************************
 BitDogLab Core Code - Adjusted for RFID and PIR Events Logging
 Target: Raspberry Pi Pico / RP2040
*******************************************************************************/

// System and Hardware Includes
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/time.h" // Necessary for get_absolute_time() and to_ms_since_boot()

// Includes the FatFs library
#include "lib/FatFs_SPI/ff15/source/ff.h"

// Include OLED display library
#include "lib_ssd1306/ssd1306.h"
#include "lib_ssd1306/ssd1306_fonts.h"

// --- UART Configuration (For communication with the Arduino Hub) ---
#define UART_ID uart0
#define BAUD_RATE 9600
#define UART_TX_PIN 0
#define UART_RX_PIN 1

// --- RGB LED Configuration ---
#define LED_RED_PIN 13
#define LED_GREEN_PIN 11
#define LED_BLUE_PIN 12

// --- I2C Configuration for OLED Display ---
#define I2C_PORT i2c0
#define I2C_SDA 4  // GPIO4 for SDA
#define I2C_SCL 5  // GPIO5 for SCL

// --- Authorized UIDs ---
const char *AUTHORIZED_UIDS[] = {
    "224c8d04",
    "b4067e05",
};
const int NUM_AUTHORIZED_UIDS = 2;

// --- Global Variables ---
FATFS fs; // File system object
FIL fil;  // File object

// --- Helper Functions ---

// Function to control the RGB LED
void set_rgb_color(int r, int g, int b) {
    gpio_put(LED_RED_PIN, r);
    gpio_put(LED_GREEN_PIN, g);
    gpio_put(LED_BLUE_PIN, b);
}

// Initialize the SD card and mount the file system
void initialize_sd() {
    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        printf("Failed to mount SD card: %d\n", fr);
        // Set red color to indicate failure
        set_rgb_color(1, 0, 0); 
    } else {
        printf("SD card mounted successfully.\n");
    }
}

// Function to record data in the log file
// Note: This log uses time since boot (minutes:seconds) instead of RTC time
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

// Function to register access events (RFID)
void log_access_event(const char* uid, const char* status) {
    char access_str[50];
    snprintf(access_str, sizeof(access_str), "UID=%s, Status=%s", uid, status);
    log_event("RFID_ACCESS", access_str);
}

// Function to register PIR events
void log_pir_event(const char* status) {
    log_event("PIR_STATUS", status);
}

// Function to update OLED display with current status
void update_oled_display(const char* rfid_status, const char* pir_status) {
    ssd1306_Fill(Black);
    
    // Title
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("BitDogLab Monitor", Font_6x8, White);
    
    // RFID Status
    ssd1306_SetCursor(0, 16);
    char rfid_line[32];
    snprintf(rfid_line, sizeof(rfid_line), "RFID: %s", rfid_status);
    ssd1306_WriteString(rfid_line, Font_6x8, White);
    
    // PIR Status
    ssd1306_SetCursor(0, 32);
    char pir_line[32];
    snprintf(pir_line, sizeof(pir_line), "PIR: %s", pir_status);
    ssd1306_WriteString(pir_line, Font_6x8, White);
    
    ssd1306_UpdateScreen();
}

int main() {
    stdio_init_all();

    // --- UART Initialization ---
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // --- GPIO Initialization (LEDs) ---
    gpio_init(LED_RED_PIN);
    gpio_init(LED_GREEN_PIN);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    
    // --- I2C Initialization for OLED ---
    i2c_init(I2C_PORT, 100 * 1000); // Initialize I2C at 100kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // --- Initialize OLED Display ---
    printf("Initializing SSD1306 OLED display...\n");
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("BitDogLab", Font_6x8, White);
    ssd1306_UpdateScreen();
    printf("OLED display initialized successfully\n");
    
    // --- SD Card Initialization ---
    initialize_sd();

    printf("BitDogLab: Waiting for Arduino data...\n");
    
    char buffer[50];
    int idx = 0;

    while (1) {
        
        // Check data from UART (RFID tag or PIR status)
        if (uart_is_readable(UART_ID)) {
            char c = uart_getc(UART_ID);
            
            if (c == '\n' || c == '\r') {
                buffer[idx] = '\0';
                
                if (idx > 0) {
                    
                    // 1. Check for PIR status (e.g., PIR_STATUS:MOTION_DETECTED)
                    if (strstr(buffer, "PIR_STATUS:") != NULL) {
                        
                        // Extract status part (e.g., MOTION_DETECTED)
                        char *status_message = buffer + strlen("PIR_STATUS:");
                        printf("Received PIR Status: %s\n", status_message);
                        log_pir_event(status_message);
                        
                        // Update OLED with PIR status
                        update_oled_display("---", status_message);
                        
                        // Flash blue for PIR detection
                        set_rgb_color(0, 0, 1);
                        sleep_ms(50);
                        set_rgb_color(0, 0, 0); 
                        
                    } 
                    // 2. Check for RFID UID (e.g., RFID_UID:b4067e05)
                    else if (strstr(buffer, "RFID_UID:") != NULL) {
                        
                        // Extract UID part
                        char *uid_str = buffer + strlen("RFID_UID:");
                        printf("Received UID: %s\n", uid_str);
                        
                        int access_granted = 0;
                        for (int i = 0; i < NUM_AUTHORIZED_UIDS; i++) {
                            if (strcmp(uid_str, AUTHORIZED_UIDS[i]) == 0) {
                                access_granted = 1;
                                break;
                            }
                        }
                        
                        if (access_granted) {
                            printf("Access Granted!\n");
                            log_access_event(uid_str, "GRANTED");
                            update_oled_display("GRANTED", "---");
                            set_rgb_color(0, 1, 0); // Green
                            sleep_ms(2000);
                            set_rgb_color(0, 0, 0); // Turn off the LEDs
                        } else {
                            printf("Access Denied!\n");
                            log_access_event(uid_str, "DENIED");
                            update_oled_display("DENIED", "---");
                            set_rgb_color(1, 0, 0); // Red
                            sleep_ms(2000);
                            set_rgb_color(0, 0, 0); // Turn off the LEDs
                        }
                    }
                    // Optional: Handle unknown lines
                    else {
                        printf("Received UNKNOWN data: %s\n", buffer);
                        log_event("UNKNOWN_DATA", buffer);
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