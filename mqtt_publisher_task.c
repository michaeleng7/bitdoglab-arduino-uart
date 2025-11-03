/*******************************************************************************
 MQTT Publisher Task - STEP 1: Wi-Fi Connectivity Test (Final Stable Version)
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/prot/iana.h"
#include "lwip/tcpip.h"
#include "pico/cyw43_arch.h"
#include "main.h"

static mqtt_client_t* mqtt_client;

// Implementation of the DNS callback function for asynchronous resolution
static void dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    ip_addr_t *target_addr = (ip_addr_t *)callback_arg;
    if (ipaddr) {
        ip_addr_copy(*target_addr, *ipaddr);
        printf("DNS: Hostname resolvido para %s\n", ipaddr_ntoa(ipaddr));
    } else {
        IP_SET_TYPE(target_addr, IPADDR_TYPE_INVALID);
        printf("DNS: Falha ao resolver hostname: %s\n", name);
    }
}

void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    printf("MQTT: Callback chamado - Status: %d\n", status);
    switch(status) {
        case MQTT_CONNECT_ACCEPTED:
            printf("MQTT: Conectado ao broker com sucesso!\n");
            break;
        case MQTT_CONNECT_DISCONNECTED:
            printf("MQTT: Conexão desconectada pelo broker\n");
            break;
        case MQTT_CONNECT_REFUSED_PROTOCOL_VERSION:
            printf("MQTT: Protocolo não suportado\n");
            break;
        case MQTT_CONNECT_REFUSED_IDENTIFIER:
            printf("MQTT: ID do cliente rejeitado\n");
            break;
        case MQTT_CONNECT_REFUSED_SERVER:
            printf("MQTT: Servidor indisponível\n");
            break;
        case MQTT_CONNECT_REFUSED_USERNAME_PASS:
            printf("MQTT: Credenciais inválidas\n");
            break;
        case MQTT_CONNECT_REFUSED_NOT_AUTHORIZED_:
            printf("MQTT: Não autorizado\n");
            break;
        default:
            printf("MQTT: Status de conexão desconhecido: %d\n", status);
            break;
    }
}

// Callback for publication result
static void mqtt_publish_cb(void *arg, err_t result) {
    if (result != ERR_OK) {
        printf("MQTT: Erro na publicação: %d\n", result);
    } else {
        printf("MQTT: Publicação bem-sucedida\n");
    }
}

void vMqttPublisherTask(void *pvParameters) {
    ip_addr_t broker_addr;
    
    printf("MQTT: Iniciando task publisher...\n");
    
    // 1. Wi-Fi standby
    while (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_UP) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 2. Initializes MQTT client
    mqtt_client = mqtt_client_new();
    if (mqtt_client == NULL) {
        printf("MQTT: Falha ao criar cliente\n");
        return;
    }

    // 3. DNS Resolution: Obtains the IP address from the hostname.
    printf("DNS: Tentando resolver hostname: %s\n", MQTT_BROKER);
    IP_SET_TYPE(&broker_addr, IPADDR_TYPE_INVALID);
    
    // Calls the DNS function (in a non-blocking/extended way)
    err_t dns_err = dns_gethostbyname(MQTT_BROKER, &broker_addr,
                                      dns_callback, &broker_addr);

    if (dns_err == ERR_OK) {
        printf("DNS: Hostname já resolvido: %s\n", ipaddr_ntoa(&broker_addr));
    } else if (dns_err != ERR_INPROGRESS) {
        printf("DNS: Erro ao iniciar resolução: %d\n", dns_err);
        return;
    }

    // Waiting for DNS resolution to complete (if it's ERR_INPROGRESS).
    TickType_t start_time = xTaskGetTickCount();
    while (ip_addr_isany(&broker_addr)) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (xTaskGetTickCount() - start_time > pdMS_TO_TICKS(5000)) {
            printf("DNS: Timeout na resolução do hostname.\n");
            return;
        }
    }
    
    // 4. Connect
    struct mqtt_connect_client_info_t ci = {
        .client_id = MQTT_CLIENT_ID,
        .keep_alive = 60,  // Reduzido para 60 segundos
        .will_topic = NULL,
        .will_msg = NULL,
        .will_retain = 0,
        .will_qos = 0
    };

    // Main connection and publishing loop
    while (true) {
        printf("MQTT: Tentando conectar ao broker %s:%d...\n", ipaddr_ntoa(&broker_addr), LWIP_IANA_PORT_MQTT);

        err_t err = mqtt_client_connect(mqtt_client, &broker_addr, LWIP_IANA_PORT_MQTT,
                                      mqtt_connection_cb, NULL, &ci);

        if (err != ERR_OK) {
            printf("MQTT: Erro na tentativa de conexão: %d\n", err);
            switch(err) {
                case ERR_VAL:
                    printf("MQTT: Parâmetros inválidos\n");
                    break;
                case ERR_ISCONN:
                    printf("MQTT: Já conectado\n");
                    break;
                case ERR_CONN:
                    printf("MQTT: Erro de conexão\n");
                    break;
                case ERR_MEM:
                    printf("MQTT: Sem memória\n");
                    break;
                default:
                    printf("MQTT: Erro desconhecido na tentativa de conexão\n");
                    break;
            }
            vTaskDelay(pdMS_TO_TICKS(10000));  // Wait 10 seconds before retrying
            continue;
        }

        printf("MQTT: Conexão iniciada, aguardando callback...\n");

        // Please wait a moment for the callback to be triggered.
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Check if the connection has been established.
        if (!mqtt_client_is_connected(mqtt_client)) {
            printf("MQTT: Conexão não estabelecida após tentativa, tentando novamente...\n");
            mqtt_disconnect(mqtt_client);
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        printf("MQTT: Conexão estabelecida, aguardando mensagens...\n");

        // Processes messages from the queue (if the code here is correct)
        MqttMessage_t msg;
        TickType_t last_status_check = xTaskGetTickCount();
        while (mqtt_client_is_connected(mqtt_client)) {
            // Checks connection status every 10 seconds.
            if (xTaskGetTickCount() - last_status_check > pdMS_TO_TICKS(10000)) {
                printf("MQTT: Verificando status da conexão...\n");
                last_status_check = xTaskGetTickCount();
            }

            if (xQueueReceive(xMqttQueue, &msg, pdMS_TO_TICKS(1000)) == pdPASS) {
                char mqtt_msg[128];
                char topic[64];

                // Define the topic based on the event type.
                if (strcmp(msg.type, "PIR") == 0) {
                    strcpy(topic, "bitdoglab/pir");
                } else if (strcmp(msg.type, "ACCESS") == 0) {
                    strcpy(topic, "bitdoglab/access");
                } else {
                    strcpy(topic, MQTT_TOPIC); // fallback to status
                }

                snprintf(mqtt_msg, sizeof(mqtt_msg),
                        "{\"type\":\"%s\",\"uid\":\"%s\",\"status\":\"%s\"}",
                        msg.type, msg.uid, msg.status);

                printf("MQTT: Publicando mensagem no tópico %s: %s\n", topic, mqtt_msg);

                err = mqtt_publish(mqtt_client, topic, mqtt_msg,
                                 strlen(mqtt_msg), 0, 0, mqtt_publish_cb, NULL);

                if (err != ERR_OK) {
                    printf("MQTT: Erro ao publicar: %d\n", err);
                }
            }
        }

        printf("MQTT: Loop de conexão terminou, desconectando...\n");
        mqtt_disconnect(mqtt_client);
        vTaskDelay(pdMS_TO_TICKS(10000));  // Wait 10 seconds before retrying
    }
}