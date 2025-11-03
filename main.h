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

// --- Wi-Fi and MQTT Configuration Constants ---
// Define the credentials and broker settings here, accessible by main.c and mqtt_publisher_task.c
#define WIFI_SSID       "MDC" 
#define WIFI_PASSWORD   "mdc1020304050" 
// Using Hostname for reliable connection (requires DNS implementation)
#define MQTT_BROKER_IP  "test.mosquitto.org" // Currently using hostname in IP slot (requires DNS)
#define MQTT_BROKER     "test.mosquitto.org"
#define MQTT_TOPIC_OUT  "bitdoglab/access/event"
#define MQTT_CLIENT_ID  "BitDogLab"
#define MQTT_TOPIC      "bitdoglab/status" // Main publication topic
// -----------------------------------


// Structure for MQTT messages
typedef struct {
    char type[16];    // Event type (ACCESS/PIR)
    char uid[16];     // Tag UID or identifier
    char status[32];  // Event status
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
extern void initialize_i2c(void); // Helper function declaration

// Task Prototypes (Required for xTaskCreate)
extern void vDisplayUpdaterTask(void *pvParameters);
extern void vUartReaderTask(void *pvParameters);
extern void vWifiConnectTask(void *pvParameters);
extern void vMqttPublisherTask(void *pvParameters);

extern cyw43_t cyw43_state; // Declaration of the global driver structure
extern int cyw43_is_connected(void); // Prototype needed for display_status
extern int cyw43_wifi_link_status(cyw43_t *self, int itf); // Full prototype

extern void set_rgb_color(int r, int g, int b);
extern void log_access_event(const char* uid, const char* status);
extern void log_pir_event(const char* status);

// Definitions
#define MAX_TAG_HISTORY 10
#define TAG_READ_TIMEOUT_MS 1000

// Structure for tag statistics
typedef struct {
    char uid[16];
    uint32_t read_attempts;
    uint32_t successful_reads;
    TickType_t last_read_time;
    uint32_t consecutive_fails;
} TagStats_t;

// Function Prototypes
void log_event(const char* event_type, const char* message);
void log_access_event(const char* uid, const char* status);
void log_pir_event(const char* status);
bool is_uid_authorized(const char* uid);
void update_tag_stats(const char* uid, bool success);
void print_tag_stats(void);
void toggle_blue_led(void);

// External Variables
extern TagStats_t tag_history[MAX_TAG_HISTORY];
extern int num_tags_tracked;
extern const char *AUTHORIZED_UIDS[];
extern const int NUM_AUTHORIZED_UIDS;

#endif // MAIN_H