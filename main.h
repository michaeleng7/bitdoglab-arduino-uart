#ifndef MAIN_H
#define MAIN_H

// --- CRITICAL CONFIGURATIONS FOR FREE RTOS CORE (Needs to be defined early) ---
// These are defined here to ensure the Pico SDK sees the correct values
// and avoids implicit declaration errors for recursive functions.
#define configUSE_RECURSIVE_MUTEXES             1
#define INCLUDE_xSemaphoreGetMutexHolder        1
#define INCLUDE_vRecursiveMutexCreate           1 
#define INCLUDE_xSemaphoreGiveRecursive         1
#define INCLUDE_xSemaphoreTakeRecursive         1
// ------------------------------------------

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "pico/cyw43_arch.h" // Needed for Wi-Fi defines

// --- Wi-Fi Configuration Constants ---
// Define the credentials here, accessible by main.c and mqtt_publisher_task.c
#define WIFI_SSID       "MDC" 
#define WIFI_PASSWORD   "mdc1020304050" 
#define MQTT_BROKER_IP  "192.168.1.10"
#define MQTT_PORT       1883
#define MQTT_TOPIC_OUT  "bitdoglab/access/event"
// -----------------------------------


// Struct for messages sent from UART Task to MQTT Task
typedef struct {
    char event_type[16];   // e.g., "RFID_ACCESS", "PIR_STATUS"
    char message_data[32]; // e.g., the UID, or the status ("GRANTED", "DENIED", "MOTION")
} MqttMessage_t;


// --- FreeRTOS Global Handles (EXPORTS) ---
// Note: These must be defined (declared without extern) in main.c
extern QueueHandle_t xMqttQueue;       
extern SemaphoreHandle_t xOledMutex;   
extern SemaphoreHandle_t xStateMutex;  

// --- Global variables for display status (EXPORTS) ---
extern char current_pir_status[32];
extern char current_uid[16];
extern char current_status[32];


// --- Function Prototypes used across files ---
extern void set_rgb_color(int r, int g, int b);
extern void log_access_event(const char* uid, const char* status);
extern void log_pir_event(const char* status);
extern void initialize_i2c(void); // Added declaration for helper function

// Task Prototypes (Required for xTaskCreate)
extern void vDisplayUpdaterTask(void *pvParameters);
extern void vUartReaderTask(void *pvParameters);
extern void vWifiConnectTask(void *pvParameters);
extern void vMqttPublisherTask(void *pvParameters);

extern cyw43_t cyw43_state; // Declaração da estrutura global do driver
extern int cyw43_is_connected(void); // Protótipo necessário para display_status
extern int cyw43_wifi_link_status(cyw43_t *self, int itf); // Protótipo completo

extern void set_rgb_color(int r, int g, int b);
extern void log_access_event(const char* uid, const char* status);
extern void log_pir_event(const char* status);

// Definições
#define MAX_TAG_HISTORY 10
#define TAG_READ_TIMEOUT_MS 1000

// Estrutura para estatísticas de tag
typedef struct {
    char uid[16];
    uint32_t read_attempts;
    uint32_t successful_reads;
    TickType_t last_read_time;
    uint32_t consecutive_fails;
} TagStats_t;

// Protótipos das funções
void log_event(const char* event_type, const char* message);
void log_access_event(const char* uid, const char* status);
void log_pir_event(const char* status);
bool is_uid_authorized(const char* uid);
void update_tag_stats(const char* uid, bool success);
void print_tag_stats(void);
void toggle_blue_led(void);

// Variáveis externas
extern TagStats_t tag_history[MAX_TAG_HISTORY];
extern int num_tags_tracked;
extern const char *AUTHORIZED_UIDS[];
extern const int NUM_AUTHORIZED_UIDS;

#endif // MAIN_H