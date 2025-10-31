/*******************************************************************************
 BitDogLab V7 Final Code - FreeRTOS Implementation
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

// FreeRTOS Headers
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h" // For Mutexes

// Includes the FatFs library
#include "lib/FatFs_SPI/ff15/source/ff.h"

// Include OLED display library
#include "lib_ssd1306/ssd1306.h"
#include "lib_ssd1306/ssd1306_fonts.h"


// --- Configuration Definitions ---
#define UART_ID uart0
#define BAUD_RATE 9600
#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define I2C_PORT i2c0
#define I2C_SDA 4
#define I2C_SCL 5

#define LED_RED_PIN 13
#define LED_GREEN_PIN 11
#define LED_BLUE_PIN 12


// --- Authorized UIDs ---
const char *AUTHORIZED_UIDS[] = {
    "224c8d04",
    "b4067e05",
};
const int NUM_AUTHORIZED_UIDS = 2;


// --- Global Variables (Protected Shared State) ---
FATFS fs; // File system object
FIL fil;  // File object

// State variables for the OLED and system status
char current_status[32] = "INITIALIZING...";
char last_uid[16] = "NONE";
char pir_current_state[32] = "NO MOTION"; 

// Mutex to protect access to global state variables
SemaphoreHandle_t xStateMutex;


// --- Function Prototypes ---
void set_rgb_color(int r, int g, int b);
void initialize_sd(void);
void log_event(const char* event_type, const char* message);
void log_access_event(const char* uid, const char* status);
void log_pir_event(const char* status);
void display_status(void);

// --- FreeRTOS Tasks Prototypes ---
void vUartReaderTask(void *pvParameters);
void vDisplayUpdaterTask(void *pvParameters);


// --- Helper Functions ---

/**
 * @brief Controls the RGB LED color.
 */
void set_rgb_color(int r, int g, int b) {
    gpio_put(LED_RED_PIN, r);
    gpio_put(LED_GREEN_PIN, g);
    gpio_put(LED_BLUE_PIN, b);
}

/**
 * @brief Initializes the SD card and mounts the file system.
 */
void initialize_sd(void) {
    FRESULT fr = f_mount(&fs, "", 1);
    
    // Acquire mutex before writing to shared state
    if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
        if (fr != FR_OK) {
            printf("Failed to mount SD card: %d\n", fr);
            strcpy(current_status, "SD CARD ERROR");
            set_rgb_color(1, 0, 0); 
        } else {
            printf("SD card mounted successfully.\n");
            strcpy(current_status, "SYSTEM READY");
        }
        xSemaphoreGive(xStateMutex);
    }
}

/**
 * @brief Records data in the log file (log.txt).
 */
void log_event(const char* event_type, const char* message) {
    // NOTE: FatFs operations are blocking and can take time, 
    // but we keep them here for direct translation.
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

/**
 * @brief Registers access events (RFID).
 */
void log_access_event(const char* uid, const char* status) {
    char access_str[50];
    snprintf(access_str, sizeof(access_str), "UID=%s, Status=%s", uid, status);
    log_event("RFID_ACCESS", access_str);
}

/**
 * @brief Registers PIR events.
 */
void log_pir_event(const char* status) {
    log_event("PIR_STATUS", status);
}


/**
 * @brief Updates OLED display with current status.
 * This function accesses global variables and MUST acquire the Mutex.
 */
void display_status(void) {
    char buffer[32];
    char local_status[32], local_uid[16], local_pir_state[32];

    // Acquire mutex to safely read shared state
    if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
        strncpy(local_status, current_status, 31);
        strncpy(local_uid, last_uid, 15);
        strncpy(local_pir_state, pir_current_state, 31);
        local_status[31] = '\0';
        local_uid[15] = '\0';
        local_pir_state[31] = '\0';
        xSemaphoreGive(xStateMutex); // Release mutex immediately after copying
    } else {
        // Handle mutex acquisition failure if necessary
        return; 
    }

    ssd1306_Fill(Black);
    
    // Line 1: Header
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("ACCESS MONITOR HUB", Font_6x8, White);

    // Line 2: Last UID read
    ssd1306_SetCursor(0, 16);
    snprintf(buffer, sizeof(buffer), "UID: %s", local_uid);
    ssd1306_WriteString(buffer, Font_6x8, White);

    // Line 3: PIR State
    ssd1306_SetCursor(0, 32);
    snprintf(buffer, sizeof(buffer), "PIR: %s", local_pir_state);
    ssd1306_WriteString(buffer, Font_6x8, White);
    
    // Line 4: Main Access/System Status
    ssd1306_SetCursor(0, 48);
    ssd1306_WriteString(local_status, Font_6x8, White); 

    ssd1306_UpdateScreen();
}


// ===================================================================
// === FREERTOS TASKS IMPLEMENTATION ===
// ===================================================================

/**
 * @brief Task responsible for reading UART data (RFID/PIR) and processing logic.
 * High Priority (2) ensures quick response to incoming data.
 */
void vUartReaderTask(void *pvParameters) {
    
    char buffer[50];
    int idx = 0;
    
    // Initialization: Must happen before the loop for the Task to work.
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    while (1) {
        
        // Check data from UART (RFID tag or PIR status)
        if (uart_is_readable(UART_ID)) {
            char c = uart_getc(UART_ID);
            
            if (c == '\n' || c == '\r') {
                buffer[idx] = '\0';
                
                if (idx > 0) {
                    
                    // 1. Check for PIR status (e.g., PIR_STATUS:MOTION_DETECTED_RFID_ACTIVATED)
                    if (strstr(buffer, "PIR_STATUS:") != NULL) {
                        
                        char *status_message = buffer + strlen("PIR_STATUS:");
                        printf("Received PIR Status: %s\n", status_message);
                        log_pir_event(status_message);
                        
                        // Acquire mutex to safely update shared state
                        if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
                            if (strstr(status_message, "ACTIVATED") != NULL) {
                               strcpy(pir_current_state, "ACTIVE");
                               strcpy(current_status, "Awaiting Tag");
                            } else if (strstr(status_message, "SLEEP") != NULL) {
                               strcpy(pir_current_state, "IDLE"); // IDLE = NO MOTION, RFID SLEEP
                               strcpy(current_status, "Awaiting Motion");
                            } else {
                               strcpy(pir_current_state, status_message);
                            }
                            xSemaphoreGive(xStateMutex);
                        }
                        
                        // Flash blue for PIR detection (immediate feedback)
                        set_rgb_color(0, 0, 1);
                        vTaskDelay(pdMS_TO_TICKS(50));
                        set_rgb_color(0, 0, 0); 
                        
                    } 
                    // 2. Check for RFID UID (e.g., RFID_UID:b4067e05)
                    else if (strstr(buffer, "RFID_UID:") != NULL) {
                        
                        char *uid_str = buffer + strlen("RFID_UID:");
                        printf("Received UID: %s\n", uid_str);
                        
                        // Update last UID globally (Protected Write)
                        if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
                            strncpy(last_uid, uid_str, sizeof(last_uid) - 1);
                            last_uid[sizeof(last_uid) - 1] = '\0';
                            xSemaphoreGive(xStateMutex);
                        }
                        
                        int access_granted = 0;
                        for (int i = 0; i < NUM_AUTHORIZED_UIDS; i++) {
                            if (strcmp(uid_str, AUTHORIZED_UIDS[i]) == 0) {
                                access_granted = 1;
                                break;
                            }
                        }
                        
                        // Update status and LED (Protected Write)
                        if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
                            if (access_granted) {
                                printf("Access Granted!\n");
                                log_access_event(uid_str, "GRANTED"); 
                                strcpy(current_status, "ACCESS GRANTED"); 
                                set_rgb_color(0, 1, 0); // Green
                                vTaskDelay(pdMS_TO_TICKS(2000)); // Non-blocking delay for the Task
                                set_rgb_color(0, 0, 0); 
                            } else {
                                printf("Access Denied!\n");
                                log_access_event(uid_str, "DENIED"); 
                                strcpy(current_status, "ACCESS DENIED"); 
                                set_rgb_color(1, 0, 0); // Red
                                vTaskDelay(pdMS_TO_TICKS(2000)); // Non-blocking delay for the Task
                                set_rgb_color(0, 0, 0); 
                            }
                            xSemaphoreGive(xStateMutex);
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
        
        // Mandatory delay to yield processing time to other tasks
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}


/**
 * @brief Task responsible for periodically updating the OLED display.
 * Low Priority (1) since it's a cosmetic function.
 */
void vDisplayUpdaterTask(void *pvParameters) {
    
    // Initialization: Must happen before the loop for the Task to work.
    i2c_init(I2C_PORT, 100 * 1000); 
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_Init();
    
    while(1) {
        display_status();
        
        // Update display every 250ms (non-blocking delay)
        vTaskDelay(pdMS_TO_TICKS(250)); 
    }
}


// ===================================================================
// === MAIN ENTRY POINT ===
// ===================================================================

int main() {
    stdio_init_all();

    // --- 1. GPIO Initialization (LEDs) ---
    gpio_init(LED_RED_PIN);
    gpio_init(LED_GREEN_PIN);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    
    // --- 2. Create Mutex for Shared State Protection ---
    xStateMutex = xSemaphoreCreateMutex();

    if (xStateMutex != NULL) {
        
        // --- 3. SD Card Initialization (Requires Mutex to update global status) ---
        initialize_sd();

        printf("BitDogLab: FreeRTOS Initializing. Waiting for Arduino data...\n");
        
        // --- 4. Create Tasks ---
        
        // UART Reader: High Priority (2)
        xTaskCreate(vUartReaderTask, "UART_Reader", 2048, NULL, 2, NULL); 
        
        // Display Updater: Low Priority (1)
        xTaskCreate(vDisplayUpdaterTask, "OLED_Updater", 1024, NULL, 1, NULL); 
        
        // --- 5. Start the FreeRTOS Scheduler ---
        vTaskStartScheduler();
        
    } else {
        printf("ERROR: Failed to create Mutex. System Halted.\n");
        // Loop forever in case of failure
        while(1) { set_rgb_color(1, 0, 0); sleep_ms(500); set_rgb_color(0, 0, 0); sleep_ms(500); }
    }

    // The code should never reach here (Scheduler takes over)
    for (;;) {} 
    return 0;
}