/*******************************************************************************
 MQTT Publisher Task - STEP 1: Wi-Fi Connectivity Test (Final Stable Version)
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// FreeRTOS includes
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

// Pico W includes (Order matters here for LwIP dependencies)
#include "pico/cyw43_arch.h"
#include "lwip/netif.h"     // Required for netif_get_ip4_addr
#include "lwip/ip4_addr.h"  // Required for ip4addr_ntoa
#include "lwip/sockets.h"

// Project includes (Main.h contains all configuration and global definitions)
#include "main.h"

struct netif; // Forward declaration for LwIP structure

// --- Configuration (These are now expected to be defined in main.h) ---
#define MAX_RECONNECT_ATTEMPTS 5


// --- Main Task Function ---

void vMqttPublisherTask(void *pvParameters) {
    (void) pvParameters;

    int reconnect_count = 0;
    
    // Note: cyw43_arch_init() is called in main().

    // 1. Setup STA mode
    cyw43_arch_enable_sta_mode();

    // 2. Connect to Wi-Fi
    printf("Wi-Fi: Connecting to %s...\n", WIFI_SSID);
    
    // Loop until connected or failed too many times
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000) && (reconnect_count < MAX_RECONNECT_ATTEMPTS)) {
        printf("ERROR: Wi-Fi connection attempt %d failed. Retrying in 5s...\n", ++reconnect_count);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    if (reconnect_count >= MAX_RECONNECT_ATTEMPTS) {
        printf("FATAL ERROR: Failed to connect to Wi-Fi. Halting task.\n");
        if (xSemaphoreTake(xStateMutex, 0) == pdPASS) {
            strcpy(current_status, "WIFI FAIL");
            xSemaphoreGive(xStateMutex);
        }
        vTaskDelete(NULL);
    }
    
    // --- Connection Success Block ---
    printf("Wi-Fi: Connected! Stabilizing network...\n");
    
    // Give network time to fully stabilize the IP
    vTaskDelay(pdMS_TO_TICKS(1000)); 
    
    // 3. Print the Local IP Address (CORRECTED USAGE)
    // We use the global cyw43_state structure exposed by pico/cyw43_arch.h
    printf("Wi-Fi: Obtained IP Address: %s\n", 
           ip4addr_ntoa(netif_ip4_addr(&cyw43_state.netif[CYW43_ITF_STA])));


    // --- Main Task Loop: Keeps Wi-Fi connection alive and awaits MQTT integration ---
    while (1) {
        // Yield periodically to allow LwIP maintenance (essential for stable connection)
        // In this architecture, LwIP is handled by FreeRTOS, but yielding helps.
        vTaskDelay(pdMS_TO_TICKS(5000)); 
    }
}