#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

// Define a porta UART e a taxa de baud
#define UART_ID uart0
#define BAUD_RATE 9600

// Define os pinos UART
#define UART_TX_PIN 0
#define UART_RX_PIN 1

int main() {
    // Initialize the comunications over USB serial
    stdio_init_all();

    // Initialize a UART
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    
    // Add a small delay to ensure the UART is ready
    sleep_ms(100);

    printf("BitDogLab está pronta para receber dados do Arduino...\n");
    
    while (1) {
        // Verify if there is data available to read
        if (uart_is_readable(UART_ID)) {
            // Lê o caractere recebido
            char c = uart_getc(UART_ID);
            
            // Print the received character to the USB serial
            printf("%c", c);
        }
    }

    return 0;
}