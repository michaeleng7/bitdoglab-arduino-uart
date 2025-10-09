#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

// Sets the UART instance and baud rate
#define UART_ID uart0
#define BAUD_RATE 9600

// Sets the UART pins
#define UART_TX_PIN 0
#define UART_RX_PIN 1

// Define the RGB LED pins
// Connections according to the Hardware Information Bank (HIB) V7.docx
#define LED_RED_PIN 13
#define LED_GREEN_PIN 11
#define LED_BLUE_PIN 12

// Sets the buzzer pin
#define BUZZER_PIN 21

// Authorized UIDs. Modify these UIDs as needed.
const char *AUTHORIZED_UIDS[] = {
    "224c8d04",
    "b4067e05",
};
const int NUM_AUTHORIZED_UIDS = 2; //The number of authorized tags

// Function to control the RGB LED
void set_rgb_color(int r, int g, int b) {
    gpio_put(LED_RED_PIN, r);
    gpio_put(LED_GREEN_PIN, g);
    gpio_put(LED_BLUE_PIN, b);
}

// Buzzer function (simple implementation for testing)
void buzzer_tone(uint32_t duration_ms) {
    gpio_put(BUZZER_PIN, 1);
    sleep_ms(duration_ms);
    gpio_put(BUZZER_PIN, 0);
}

int main() {
    // Initializes USB communication for the terminal
    stdio_init_all();

    // Initialize the UART
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Configures the LED pins as output
    gpio_init(LED_RED_PIN);
    gpio_init(LED_GREEN_PIN);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    
    // Configure the buzzer pin as output
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);

    printf("BitDogLab: Waiting for Arduino UID...\n");
    
    char buffer[50];
    int idx = 0;

    while (1) {
        if (uart_is_readable(UART_ID)) {
            char c = uart_getc(UART_ID);
            
            if (c == '\n' || c == '\r') {
                buffer[idx] = '\0'; // Ends the string
                
                // Adds a check for empty or initialization strings
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
                        set_rgb_color(0, 1, 0); // Green
                        buzzer_tone(250);
                        sleep_ms(2000);
                        set_rgb_color(0, 0, 0); // Turn off the LEDs
                    } else {
                        printf("Access Denied!\n");
                        set_rgb_color(1, 0, 0); // Red
                        buzzer_tone(500);
                        sleep_ms(2000);
                        set_rgb_color(0, 0, 0); // Turn off the LEDs
                    }
                }
                idx = 0; // Prepares the buffer for the next UID
            } else {
                if (idx < 49) { // Protects against buffer overflow
                    buffer[idx++] = c;
                }
            }
        }
    }

    return 0;
}