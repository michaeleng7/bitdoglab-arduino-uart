/*******************************************************************************
 BitDogLab V7 Final Code - Detailed OLED Display + RFID/PIR Logging
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
#include "pico/time.h"

// Includes the FatFs library
#include "lib/FatFs_SPI/ff15/source/ff.h"

// Include OLED display library
#include "lib_ssd1306/ssd1306.h"
#include "lib_ssd1306/ssd1306_fonts.h"

// --- UART Configuration (Arduino Hub - Mapped to GPIO 0 & 1) ---
// Note: Arduino Hub TX must be connected to Pico RX (GPIO 1), and vice-versa.
#define UART_ID uart0
#define BAUD_RATE 9600
#define UART_TX_PIN 0
#define UART_RX_PIN 1

// --- I2C Configuration (OLED Display - I2C0 Mapped to GPIO 4 & 5) ---
#define I2C_PORT i2c0
#define I2C_SDA 4  // GPIO 4 (SDA) -> For OLED Display
#define I2C_SCL 5  // GPIO 5 (SCL) -> For OLED Display

// --- RGB LED Configuration ---
#define LED_RED_PIN 13
#define LED_GREEN_PIN 11
#define LED_BLUE_PIN 12

// --- Authorized UIDs ---
const char *AUTHORIZED_UIDS[] = {
    "224c8d04",
    "b4067e05",
};
const int NUM_AUTHORIZED_UIDS = 2;

// --- Global Variables (for State Management) ---
FATFS fs; 
FIL fil;  

// Variables to hold the current status for the OLED
char current_status[32] = "INITIALIZING...";
char last_uid[16] = "NONE";
char pir_current_state[32] = "NO MOTION"; // Tracks the PIR status for the display

// --- Function Prototypes ---
void set_rgb_color(int r, int g, int b);
void initialize_sd();
void log_event(const char* event_type, const char* message);
void log_access_event(const char* uid, const char* status);
void log_pir_event(const char* status);
void display_status(); // New centralized display function

// --- Helper Functions ---

void set_rgb_color(int r, int g, int b) {
    gpio_put(LED_RED_PIN, r);
    gpio_put(LED_GREEN_PIN, g);
    gpio_put(LED_BLUE_PIN, b);
}

void initialize_sd() {
    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        printf("Failed to mount SD card: %d\n", fr);
        strcpy(current_status, "SD CARD ERROR");
        set_rgb_color(1, 0, 0); 
    } else {
        printf("SD card mounted successfully.\n");
        strcpy(current_status, "SYSTEM READY");
    }
}

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

void log_access_event(const char* uid, const char* status) {
    char access_str[50];
    snprintf(access_str, sizeof(access_str), "UID=%s, Status=%s", uid, status);
    log_event("RFID_ACCESS", access_str);
}

void log_pir_event(const char* status) {
    log_event("PIR_STATUS", status);
}

// === Function to update the OLED Display with detailed status ===
void display_status() {
    char buffer[32];
    ssd1306_Fill(Black);
    
    // Line 1: Header
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("ACCESS MONITOR HUB", Font_6x8, White);

    // Line 2: Last UID read
    ssd1306_SetCursor(0, 16);
    snprintf(buffer, sizeof(buffer), "UID: %s", last_uid);
    ssd1306_WriteString(buffer, Font_6x8, White);

    // Line 3: PIR State
    ssd1306_SetCursor(0, 32);
    snprintf(buffer, sizeof(buffer), "PIR: %s", pir_current_state);
    ssd1306_WriteString(buffer, Font_6x8, White);
    
    // Line 4: Main Access/System Status
    ssd1306_SetCursor(0, 48);
    // Use a larger or emphasized font for the main status if available, 
    // but sticking to Font_6x8 for consistency with provided files.
    ssd1306_WriteString(current_status, Font_6x8, White); 

    ssd1306_UpdateScreen();
}


int main() {
    stdio_init_all();

    // --- UART Initialization (GPIO 0 & 1) ---
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // --- I2C Initialization for OLED (I2C0, GP4/GP5) ---
    i2c_init(I2C_PORT, 100 * 1000); 
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // --- OLED Display Initialization ---
    ssd1306_Init();
    
    // --- GPIO Initialization (LEDs) ---
    gpio_init(LED_RED_PIN);
    gpio_init(LED_GREEN_PIN);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    
    // --- SD Card Initialization ---
    initialize_sd();

    printf("BitDogLab: System initialized. Waiting for Arduino data on GPIO 0/1...\n");
    
    char buffer[50];
    int idx = 0;

    // Initial display update
    display_status();


    while (1) {
        
        // Check data from UART
        if (uart_is_readable(UART_ID)) {
            char c = uart_getc(UART_ID);
            
            if (c == '\n' || c == '\r') {
                buffer[idx] = '\0';
                
                if (idx > 0) {
                    
                    // 1. Check for PIR status
                    if (strstr(buffer, "PIR_STATUS:") != NULL) {
                        
                        char *status_message = buffer + strlen("PIR_STATUS:");
                        printf("Received PIR Status: %s\n", status_message);
                        log_pir_event(status_message);
                        
                        // Update PIR display status (e.g., ACTIVATED, SLEEP)
                        if (strstr(status_message, "ACTIVATED") != NULL) {
                           strcpy(pir_current_state, "ACTIVE");
                           strcpy(current_status, "Awaiting Tag");
                        } else if (strstr(status_message, "SLEEP") != NULL) {
                           strcpy(pir_current_state, "IDLE");
                           strcpy(current_status, "Awaiting Motion");
                        } else {
                           strcpy(pir_current_state, status_message);
                        }
                        
                        // Flash blue for PIR detection
                        set_rgb_color(0, 0, 1);
                        sleep_ms(50);
                        set_rgb_color(0, 0, 0); 
                        
                    } 
                    // 2. Check for RFID UID
                    else if (strstr(buffer, "RFID_UID:") != NULL) {
                        
                        char *uid_str = buffer + strlen("RFID_UID:");
                        printf("Received UID: %s\n", uid_str);
                        
                        // Update last UID globally
                        strncpy(last_uid, uid_str, sizeof(last_uid) - 1);
                        last_uid[sizeof(last_uid) - 1] = '\0';
                        
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
                            strcpy(current_status, "ACCESS GRANTED"); 
                            set_rgb_color(0, 1, 0); // Green
                            sleep_ms(2000);
                            set_rgb_color(0, 0, 0); 
                        } else {
                            printf("Access Denied!\n");
                            log_access_event(uid_str, "DENIED"); 
                            strcpy(current_status, "ACCESS DENIED"); 
                            set_rgb_color(1, 0, 0); // Red
                            sleep_ms(2000);
                            set_rgb_color(0, 0, 0); 
                        }
                    }
                }
                idx = 0;
            } else {
                if (idx < 49) {
                    buffer[idx++] = c;
                }
            }
        }

        // Update display periodically (e.g., every 250ms)
        static uint64_t last_display_update = 0;
        uint64_t current_time_ms = to_ms_since_boot(get_absolute_time());
        if (current_time_ms - last_display_update > 250) {
            display_status();
            last_display_update = current_time_ms;
        }

        sleep_ms(1); 
    }

    return 0;
}