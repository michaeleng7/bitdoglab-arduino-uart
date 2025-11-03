/*******************************************************************************
 BitDogLab V7 Final Code - Stable FreeRTOS, OLED, SD, UART and MQTT Integration
 Target: Raspberry Pi Pico W / RP2040
 
 NOTE: The global variables and structs are declared in main.h
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/time.h"
#include "pico/cyw43_arch.h"
#include "hardware/structs/systick.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

// FreeRTOS includes
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// Includes the FatFs library (SD Card)
#include "lib/FatFs_SPI/ff15/source/ff.h"

// Include OLED display library
#include "lib_ssd1306/ssd1306.h"
#include "lib_ssd1306/ssd1306_fonts.h"

// Project Header (Contains all global definitions, structs, and FreeRTOS defines)
#include "main.h" 


// --- Configuration Definitions (Local) ---
#define UART_ID         uart0
#define BAUD_RATE       9600
#define UART_TX_PIN     0
#define UART_RX_PIN     1

#define I2C_PORT        i2c0
#define I2C_SDA_PIN     4  
#define I2C_SCL_PIN     5  

#define LED_RED_PIN     13 
#define LED_GREEN_PIN   11 
#define LED_BLUE_PIN    12 

#define BLINK_INTERVAL_MS 500  // 500ms interval for blinking


// --- Global Variable Definitions (Defined here, declared 'extern' in main.h) ---
FATFS fs; 
FIL fil;  

QueueHandle_t xMqttQueue;       
SemaphoreHandle_t xOledMutex;   
SemaphoreHandle_t xStateMutex; 

char current_pir_status[32] = "NO MOTION";
char current_uid[16] = "NONE";
char current_status[32] = "SYSTEM READY";

// --- Authorized UIDs ---
const char *AUTHORIZED_UIDS[] = {
    "224c8d04",
    "b4067e05",
};
const int NUM_AUTHORIZED_UIDS = 2;

// Global variables for statistics
TagStats_t tag_history[MAX_TAG_HISTORY] = {0};
int num_tags_tracked = 0;

static bool led_blink_state = false;


// --- Function Prototypes for Tasks (These are required for the initial calls) ---
void init_peripherals(void);
void initialize_i2c(void);
void initialize_oled(void);
void initialize_uart(void);
void initialize_sd(void);
void display_status(void);
void vUartReaderTask(void *pvParameters);
void vDisplayUpdaterTask(void *pvParameters);
void vWifiConnectTask(void *pvParameters);
extern void vMqttPublisherTask(void *pvParameters); 


// --- Helper Functions Implementation ---

/**
 * @brief Controls the onboard RGB LED color.
 */
void set_rgb_color(int r, int g, int b) {
    gpio_put(LED_RED_PIN, r);     // 1 = LED on, 0 = LED off
    gpio_put(LED_GREEN_PIN, g);   // Straightforward logic, like in the old code.
    gpio_put(LED_BLUE_PIN, b);    // No inversion
}

/**
 * @brief Initializes the I2C peripheral for the OLED display.
 */
void initialize_i2c(void) {
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

/**
 * @brief Initializes the OLED display.
 */
void initialize_oled(void) {
    ssd1306_Init(); 
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();
}

/**
 * @brief Initializes the UART peripheral.
 */
void initialize_uart(void) {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

/**
 * @brief Initializes all necessary hardware peripherals.
 */
void init_peripherals() {
    // Initialize RGB LED pins
    gpio_init(LED_RED_PIN);
    gpio_init(LED_GREEN_PIN);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    set_rgb_color(0, 0, 0); 
    
    // Initialize I2C and UART
    initialize_uart();
    initialize_i2c();
}

/**
 * @brief Initializes and mounts the SD card using FatFs.
 */
void initialize_sd(void) {
    FRESULT fr = f_mount(&fs, "0:", 1);

    if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
        if (fr == FR_OK) {
            printf("SD Card: Sistema de arquivos montado com sucesso.\n");
            
            // Tenta criar um log inicial
            log_event("SYSTEM", "Sistema inicializado");
            
            if (strcmp(current_status, "SYSTEM INIT") == 0) {
                strcpy(current_status, "SYSTEM READY");
            }
        } else {
            printf("SD Card: Falha ao montar sistema de arquivos. Erro: %d\n", fr);
            strcpy(current_status, "SD CARD ERROR");
            set_rgb_color(1, 0, 0);
        }
        xSemaphoreGive(xStateMutex);
    }
}

/**
 * @brief Função helper para obter timestamp
 */
void get_timestamp(char* buffer, size_t size) {
    time_t now;
    time(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", localtime(&now));
}

/**
 * @brief Records data in the log file (ACCESS.LOG/PIR.LOG).
 * (Placeholder for actual logging logic)
 */
void log_event(const char* event_type, const char* message) {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    if (f_mount(&fs, "0:", 1) == FR_OK) {
        FRESULT fr;
        FIL file;
        
        fr = f_open(&file, "system.log", FA_WRITE | FA_OPEN_APPEND | FA_OPEN_ALWAYS);
        if (fr == FR_OK) {
            char log_line[256];
            snprintf(log_line, sizeof(log_line), "[%s] %s: %s\n", 
                    timestamp, event_type, message);
            
            unsigned int bw;
            f_write(&file, log_line, strlen(log_line), &bw);
            f_sync(&file);
            f_close(&file);
            
            printf("Log salvo: %s", log_line);
        } else {
            printf("Erro ao abrir arquivo de log: %d\n", fr);
        }
        
        f_mount(0, "0:", 0);
    } else {
        printf("Erro ao montar sistema de arquivos\n");
    }
}

void log_access_event(const char* uid, const char* status) {
    char message[100];
    snprintf(message, sizeof(message), "UID: %s - Status: %s", uid, status);
    log_event("ACCESS", message);
}

void log_pir_event(const char* status) {
    log_event("PIR", status);
}

bool is_uid_authorized(const char* uid) {
    for(int i = 0; i < NUM_AUTHORIZED_UIDS; i++) {
        if(strcmp(uid, AUTHORIZED_UIDS[i]) == 0) {
            return true;
        }
    }
    return false;
}

void update_tag_stats(const char* uid, bool success) {
    int tag_index = -1;
    
    // Search for the tag in the history.
    for(int i = 0; i < num_tags_tracked; i++) {
        if(strcmp(uid, tag_history[i].uid) == 0) {
            tag_index = i;
            break;
        }
    }
    
    // If it's a new tag and there's still space
    if(tag_index == -1 && num_tags_tracked < MAX_TAG_HISTORY) {
        tag_index = num_tags_tracked++;
        strncpy(tag_history[tag_index].uid, uid, 15);
        tag_history[tag_index].uid[15] = '\0';
    }
    
    //Update statistics if you found or created the tag.
    if(tag_index >= 0) {
        TagStats_t* stats = &tag_history[tag_index];
        TickType_t current_time = xTaskGetTickCount();
        
        stats->read_attempts++;
        if(success) {
            stats->successful_reads++;
            stats->consecutive_fails = 0;
        } else {
            stats->consecutive_fails++;
        }
        stats->last_read_time = current_time;
        
        // Print updated statistics.
        printf("\n=== Estatísticas da Tag %s ===\n", uid);
        printf("Tentativas totais: %lu\n", stats->read_attempts);
        printf("Leituras com sucesso: %lu (%.1f%%)\n", 
               stats->successful_reads,
               (float)stats->successful_reads * 100 / stats->read_attempts);
        printf("Falhas consecutivas: %lu\n", stats->consecutive_fails);
        printf("=============================\n\n");
    }
}

void toggle_blue_led(void) {
    led_blink_state = !led_blink_state;
    set_rgb_color(0, 0, led_blink_state ? 1 : 0); // Lógica direta
}

/**
 * @brief Updates the OLED display with the current system status.
 */
void vDisplayUpdaterTask(void *pvParameters) {
    (void) pvParameters;
    const TickType_t xDisplayDelay = pdMS_TO_TICKS(100); // Mudado de 250 para 100ms

    // Get local references to font structures
    const SSD1306_Font_t font_small = Font_6x8;
    const SSD1306_Font_t font_large = Font_11x18; 

    // --- 1. OLED Initialization (Must acquire Mutex) ---
    // Initialize I2C is done in init_peripherals(), but OLED init is done here 
    if (xSemaphoreTake(xOledMutex, portMAX_DELAY) == pdPASS) { 
        initialize_oled(); 
        xSemaphoreGive(xOledMutex);
    }
    
    while (true) {
        // 1. Update display with current status
        display_status();
        
        // 2. Task delay 
        vTaskDelay(xDisplayDelay);
    }
}

/**
 * @brief Draws the current system status to the OLED display.
 */
void display_status(void) {
    // Get local references to font structures
    const SSD1306_Font_t font_small = Font_6x8;
    const SSD1306_Font_t font_large = Font_11x18; 

    char local_status[32], local_uid[16], local_pir_state[32];
    char wifi_status[16]; // Buffer for Wi-Fi status
    
    // 1. Acquire State Mutex to safely read global state variables
    if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(10)) == pdPASS) {
        strncpy(local_status, current_status, 31);
        strncpy(local_uid, current_uid, 15);
        strncpy(local_pir_state, current_pir_status, 31);
        local_status[31] = '\0';
        local_uid[15] = '\0';
        local_pir_state[31] = '\0';
        
        // Adds debugging to the serial port.
        printf("Status Atual: %s | UID: %s | PIR: %s\n", 
               local_status, local_uid, local_pir_state);
        
        xSemaphoreGive(xStateMutex);
    } else {
        // Cannot read state, use placeholders
        strcpy(local_status, "LOCK FAIL");
        strcpy(local_uid, "N/A");
    }
    
    // 2. Check Wi-Fi status (using only cyw43_tcpip_link_status)
    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
        sprintf(wifi_status, "Connected");
    } else {
        sprintf(wifi_status, "Disconnected");
    }

    // 3. Draw display buffer (Acquire OLED Mutex for drawing)
    if (xSemaphoreTake(xOledMutex, pdMS_TO_TICKS(50)) == pdPASS) {
        ssd1306_Fill(Black);

        // Line 1: Header
        ssd1306_SetCursor(0, 0); 
        ssd1306_WriteString("ACCESS MONITOR HUB", font_small, White);

        // Line 2: PIR Status
        ssd1306_SetCursor(0, 10);
        ssd1306_WriteString("PIR:", font_small, White);
        ssd1306_SetCursor(30, 10);
        ssd1306_WriteString(local_pir_state, font_small, White);

        // Line 3: UID
        ssd1306_SetCursor(0, 20);
        ssd1306_WriteString("UID:", font_small, White);
        ssd1306_SetCursor(30, 20);
        ssd1306_WriteString(local_uid, font_small, White);
        
        // Line 4: Main Access/System Status (system status)
        ssd1306_SetCursor(0, 35);
        ssd1306_WriteString("STATUS:", font_small, White);
        ssd1306_SetCursor(45, 35);
        
        
        char system_status[32];
        if (strcmp(current_uid, "NONE") == 0) {
            strcpy(system_status, "WAITING TAG");
        } else {
            strcpy(system_status, "TAG DETECTED");
        }
        ssd1306_WriteString(system_status, font_small, White);
        
        // Line 5: Wi-Fi Status
        ssd1306_SetCursor(0, 50);
        ssd1306_WriteString("WIFI:", font_small, White);
        ssd1306_SetCursor(30, 50);
        ssd1306_WriteString(wifi_status, font_small, White);
        
        ssd1306_UpdateScreen();
        xSemaphoreGive(xOledMutex); // Release OLED Mutex
    }
}

/**
 * @brief Task for reading serial data from the Arduino Hub (PIR/RFID).
 */
void vUartReaderTask(void *pvParameters) {
    (void) pvParameters;
    const TickType_t xUartDelay = pdMS_TO_TICKS(20);
    const TickType_t xUidClearDelay = pdMS_TO_TICKS(3000); // 3 seconds to clear the UID
    char rx_buffer[256] = {0};
    int rx_index = 0;
    TickType_t xLastUidTime = 0;  // Stores when the last UID was read.
    bool uidPresent = false;      // Flag to check if a UID is present.
    
    printf("UART_Reader: Task iniciada.\n");

    while (true) {
        // Check if it's time to clear the UID.
        if (uidPresent && (xTaskGetTickCount() - xLastUidTime > xUidClearDelay)) {
            if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                strcpy(current_uid, "NONE");
                uidPresent = false;
                printf("UID limpo após timeout\n");
                xSemaphoreGive(xStateMutex);
            }
        }

        while (uart_is_readable(UART_ID)) {
            char ch = uart_getc(UART_ID);
            
            if (ch != '\n' && ch != '\r') {
                rx_buffer[rx_index++] = ch;
                if (rx_index >= sizeof(rx_buffer) - 1) {
                    rx_index = 0;
                }
            } 
            else if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';
                printf("UART Recebido: %s\n", rx_buffer);
                
                if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    if (strstr(rx_buffer, "PIR_STATUS:MOTION_DETECTED")) {
                        strcpy(current_pir_status, "MOTION DETECTED");
                        toggle_blue_led();  // Starts blinking
                        log_pir_event("MOTION DETECTED");

                        // Send MQTT message for PIR motion detected
                        MqttMessage_t mqtt_msg;
                        strcpy(mqtt_msg.type, "PIR");
                        strcpy(mqtt_msg.uid, "");
                        strcpy(mqtt_msg.status, "MOTION_DETECTED");
                        xQueueSend(xMqttQueue, &mqtt_msg, pdMS_TO_TICKS(100));
                    }
                    else if (strstr(rx_buffer, "PIR_STATUS:NO_MOTION")) {
                        strcpy(current_pir_status, "NO MOTION");
                        set_rgb_color(0, 0, 0);  // Turn off all LEDs.
                        log_pir_event("NO MOTION");

                        // Send MQTT message for PIR no motion
                        MqttMessage_t mqtt_msg;
                        strcpy(mqtt_msg.type, "PIR");
                        strcpy(mqtt_msg.uid, "");
                        strcpy(mqtt_msg.status, "NO_MOTION");
                        xQueueSend(xMqttQueue, &mqtt_msg, pdMS_TO_TICKS(100));
                    }
                    else if (strstr(rx_buffer, "UID:")) {
                        char *uid_start = strstr(rx_buffer, "UID:") + 4;
                        while (*uid_start == ' ') uid_start++;

                        // Check if enough time has passed since the last reading.
                        bool read_allowed = true;
                        for(int i = 0; i < num_tags_tracked; i++) {
                            if(strcmp(uid_start, tag_history[i].uid) == 0) {
                                TickType_t time_since_last = xTaskGetTickCount() - tag_history[i].last_read_time;
                                if(time_since_last < pdMS_TO_TICKS(TAG_READ_TIMEOUT_MS)) {
                                    read_allowed = false;
                                    break;
                                }
                            }
                        }

                        if(read_allowed) {
                            strncpy(current_uid, uid_start, 15);
                            current_uid[15] = '\0';
                            xLastUidTime = xTaskGetTickCount();
                            uidPresent = true;

                            // Checks if the tag is authorized and configures the appropriate LED.
                            bool authorized = is_uid_authorized(current_uid);
                            if (authorized) {
                                set_rgb_color(0, 1, 0);  // Green for authorized access
                            } else {
                                set_rgb_color(1, 0, 0);  // Red for access denied
                            }

                            // Add access log
                            log_access_event(current_uid,
                                is_uid_authorized(current_uid) ? "AUTHORIZED" : "UNAUTHORIZED");

                            // Update statistics
                            update_tag_stats(current_uid, true);

                            // Send MQTT message for UID access
                            MqttMessage_t mqtt_msg;
                            strcpy(mqtt_msg.type, "ACCESS");
                            strcpy(mqtt_msg.uid, current_uid);
                            strcpy(mqtt_msg.status, authorized ? "AUTHORIZED" : "UNAUTHORIZED");
                            xQueueSend(xMqttQueue, &mqtt_msg, pdMS_TO_TICKS(100));

                            printf("Nova UID detectada: %s (será limpa em 3 segundos)\n", current_uid);
                        } else {
                            // Updates statistics as needed (very fast reading)
                            update_tag_stats(uid_start, false);
                            printf("Leitura ignorada - muito rápida para a mesma tag\n");
                        }
                    }

                    xSemaphoreGive(xStateMutex);
                }
                
                rx_index = 0;
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        
        vTaskDelay(xUartDelay);
    }
}

/**
 * @brief Task for initial Wi-Fi connection and launching the MQTT task.
 */
void vWifiConnectTask(void *pvParameters) {
    (void) pvParameters;
    const TickType_t xWifiRetryDelay = pdMS_TO_TICKS(5000); // 5 segundos entre tentativas
    int retry_count = 0;
    bool mqtt_task_created = false;
    
    printf("Wi-Fi_Connect: Initializing Wi-Fi...\n");

    while (true) {
        // Check status using cyw43_tcpip_link_status
        if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_UP) {
            printf("Wi-Fi_Connect: Tentativa %d de conexão Wi-Fi (SSID: %s)...\n", ++retry_count, WIFI_SSID);
            
            if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdPASS) {
                strcpy(current_status, "CONNECTING WIFI");
                xSemaphoreGive(xStateMutex);
            }
            
            cyw43_arch_enable_sta_mode();
            
            if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, 
                CYW43_AUTH_WPA2_AES_PSK, 10000)) {
                
                printf("Wi-Fi_Connect: Falha na conexão.\n");
                if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdPASS) {
                    strcpy(current_status, "WIFI CONNECT FAILED");
                    xSemaphoreGive(xStateMutex);
                }
            } else {
                printf("Wi-Fi_Connect: Conectado com sucesso!\n");
                if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdPASS) {
                    strcpy(current_status, "WIFI CONNECTED");
                    xSemaphoreGive(xStateMutex);
                }
                
                // Cria a task MQTT apenas na primeira conexão bem-sucedida
                if (!mqtt_task_created) {
                    xTaskCreate(vMqttPublisherTask, "MQTT_Publisher", 4096, NULL, 4, NULL);
                    mqtt_task_created = true;
                }
            }
        }
        
        vTaskDelay(xWifiRetryDelay);
    }
}

// ===================================================================
// === MAIN ENTRY POINT (Program Start) ===
// ===================================================================

int main() {
    stdio_init_all();

    // 1. Initialize non-FreeRTOS peripherals (GPIO, I2C, UART)
    init_peripherals();
    
    // 2. Initialize Pico W Wi-Fi Arch (Must be done before any network calls)
    if (cyw43_arch_init() != 0) {
        printf("ERROR: CYW43 Wi-Fi Arch initialization failed. Stopping.\n");
        return 1;
    }
    printf("BitDogLab: FreeRTOS/Wi-Fi Initializing...\n");

    // 3. Create FreeRTOS objects (Mutexes and Queue)
    xStateMutex = xSemaphoreCreateMutex(); // Mutex for shared status
    xOledMutex = xSemaphoreCreateMutex(); // Mutex for OLED drawing
    xMqttQueue = xQueueCreate(5, sizeof(MqttMessage_t)); 

    if (xStateMutex == NULL || xOledMutex == NULL || xMqttQueue == NULL) {
        printf("FATAL ERROR: Failed to create FreeRTOS synchronization objects.\n");
        return 1;
    }
    
    // 4. Initialize SD Card (Runs logic, uses Mutex)
    initialize_sd();
    
    printf("BitDogLab: Starting tasks...\n");
    
    // 5. Create Tasks
    xTaskCreate(vUartReaderTask, "UART_Reader", 2048, NULL, 3, NULL); // Increased priority
    xTaskCreate(vDisplayUpdaterTask, "OLED_Updater", 1024, NULL, 1, NULL); 
    xTaskCreate(vWifiConnectTask, "WIFI_Connect", 2048, NULL, 2, NULL); 
    // Note: vMqttPublisherTask is created inside vWifiConnectTask
    
    // 6. Start the FreeRTOS Scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    printf("ERROR: Scheduler stopped unexpectedly!\n");
    return 0;
}


// --- Required FreeRTOS Hook Functions (Implementation is required for successful compilation) ---

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    printf("ERROR: Stack Overflow in Task: %s\n", pcTaskName);
    for(;;);
}

void vApplicationMallocFailedHook(void) {
    printf("ERROR: Malloc Failed (FreeRTOS Heap)\n");
    for(;;);
}

#if (configSUPPORT_STATIC_ALLOCATION == 1)
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize ) {
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
#endif