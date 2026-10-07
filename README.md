# Access Monitor Hub 🛡️📡
### Sistema de Monitoramento e Controle de Acesso para Ambientes Restritos

[![EmbarcaTech](https://img.shields.io/badge/Programa-EmbarcaTech-0056b3?style=for-the-badge)](#)
[![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS-informational?style=for-the-badge&logo=freertos)](#)
[![Platform](https://img.shields.io/badge/Hardware-RP2040%20%7C%20Arduino-blue?style=for-the-badge)](#)
[![MQTT](https://img.shields.io/badge/Protocol-MQTT-purple?style=for-the-badge&logo=mqtt)](#)

---

## 📌 Sobre o Projeto

O **Access Monitor Hub** é uma solução de sistema embarcado distribuído desenvolvida para gerenciar, registrar e auditar entradas e saídas em ambientes restritos. 

O projeto soluciona problemas clássicos de **travamento sequencial de rotinas (blocking code)** e **alto consumo contínuo** por meio de:
1. **Ativação sob demanda (Low Power):** O sensor PIR detecta presença antes de acordar o módulo de leitura RFID.
2. **Arquitetura Híbrida Distribuída:** Separação entre o processamento em tempo real de sensores físicos (Arduino Uno) e o gateway de conectividade/segurança (BitDogLab / Raspberry Pi Pico W).
3. **Escalonamento Multitarefa com FreeRTOS:** Execução determinística sem bloqueio entre tarefas críticas (aquisição serial, persistência de arquivos, display e conectividade TCP/IP).
4. **Dupla Redundância de Auditoria:** Carimbo de tempo com sincronização NTP/RTC, gravando os registros localmente via FATFS (Cartão SD) e transmitindo em tempo real via **MQTT** para um painel **Node-RED**.

---

## 🎯 Desafio & Como Foi Resolvido

| Problemática Identificada | Como o Projeto Resolveu |
| :--- | :--- |
| **Gargalo de Processamento Sequencial:** Em laços tradicionais (*super-loops*), requisições de rede (Wi-Fi, NTP) prendem o processador, gerando perda de leitura de cartões. | Implementação do **FreeRTOS** no RP2040 com filas (*queues*) e priorização de tarefas, garantindo que o monitoramento serial nunca seja interrompido por rotinas de rede. |
| **Integridade Temporal & Auditoria:** Falhas de rede ou energia podem corromper logs e comprometer relatórios. | Sincronização automática via **NTP** e redundância com RTC local (DS1302). Os logs são salvos em formato tabular em **Cartão SD (FATFS)** e enviados via MQTT. |
| **Consumo Excessivo por Polling:** Manter a antena RFID emitindo e verificando constantemente drena energia desnecessária. | Uso de um **PIR (HC-SR501)** como gatilho de *wake-up*: o RFID permanece em estado de *sleep* e só é ativado na presença de pessoas. |
| **Ruídos e Perdas na Troca de Mensagens:** Falhas na comunicação de dados brutos entre circuitos. | Criação de um protocolo serial **UART robusto** entre Arduino e Pico W com pacotes estruturados e filtrados. |

---

## 🏗️ Arquitetura do Sistema

<img src=images/flowchart.png width="100%" alt="Diagrama de Arquitetura do Sistema">

### Divisão de Tarefas no FreeRTOS (Pico W)

* **Task de Aquisição UART (Prioridade Alta):** Escuta a interface serial contínua para captura imediata da UID enviada pelo Arduino.
* **Task de Interface & Feedback (Prioridade Média):** Controla o display OLED SSD1306 e o LED RGB (Verde = Acesso Liberado, Vermelho = Negado, Azul = Movimento Detectado).
* **Task de Persistência (FATFS):** Grava logs com carimbo de data/hora no cartão micro SD.
* **Task de Conectividade & Telemetria:** Consome eventos de uma fila de mensagens e envia pacotes JSON via MQTT sem bloquear o sistema.

<img src=images/overall-project.jpg width="100%" alt="Visão Geral do Projeto">

---

## 🔌 Pinagem e Diagrama de Conexões

### 1. Hub de Sensores (Arduino Uno)
* **Módulo RFID (MFRC522) [SPI]:**
  * `SDA (SS)`: D10 | `SCK`: D13 | `MOSI`: D11 | `MISO`: D12 | `RST`: D9
* **Sensor PIR (HC-SR501):**
  * `OUT`: D2
* **Módulo RTC DS1302:**
  * `ENA (RST)`: D5 | `CLK`: D3 | `DAT (I/O)`: D4
* **Alimentação:** Sensores alimentados em seus respectivos níveis (5V / 3.3V) e **GND comum**.

### 2. Gateway IoT (BitDogLab / Raspberry Pi Pico W)
* **Display OLED (SSD1306) [I2C]:**
  * `SDA`: GPIO 4 | `SCL`: GPIO 5
* **LED RGB:**
  * `Vermelho`: GPIO 13 | `Verde`: GPIO 11 | `Azul`: GPIO 12
* **Módulo SD Card [SPI Secundário]:**
  * Conectado aos pinos SPI da placa com suporte FATFS.

### 3. Comunicação Inter-Placas (UART Cruzada)
* **Arduino TX (D1)** $\rightarrow$ **BitDogLab RX (GPIO 1)**
* **Arduino RX (D0)** $\leftarrow$ **BitDogLab TX (GPIO 0)**
* ⚠️ **GND:** Ponto de terra unificado obrigatório entre ambas as placas.

---

## 📊 Formato dos Dados & Telemetria

### Tópicos MQTT
* `bitdoglab/access` : Eventos de acesso de cartões autorizados ou não autorizados.
* `bitdoglab/pir` : Status do sensor de presença e acionamento do RFID.
* `bitdoglab/status` : Heartbeat e conectividade geral da controladora.

### Exemplo de Payload JSON (MQTT)
```json
{
  "type": "ACCESS",
  "uid": "b4067e05",
  "status": "AUTHORIZED"
}
```
<img src=images/mqtt.jpg width="100%" alt="Exemplo de Payload MQTT">

### Exemplo de Log Local no Cartão SD

<img src=images/log-microsd.jpg width="100%" alt="Exemplo de Log Local no Cartão SD">

---

## 🖥️ Demonstração e Resultados

* **Logs de Sistema & Monitor Serial:** Rastreabilidade completa de tentativas com estatísticas de leitura.

<img src=images/log-acess-authorized.jpg width="100%" alt="Log de Acesso Autorizado">

<img src=images/log-acess-unauthorized.png width="100%" alt="Log de Acesso Não Autorizado">

* **Interface Visual Integrada:** Feedback em tempo real via Display OLED e matriz/LEDs RGB na BitDogLab.

<img src=images/acess-authorized-led.jpg width="100%" alt="Acesso Autorizado LED Verde">

<img src=images/acess-denied-led.jpg width="100%" alt="Acesso Negado LED Vermelho">

* **Dashboard Node-RED:** Painel em nuvem/local para monitoramento de segurança em tempo real.

<img src=images/node-red-dashboard-status-no-motion.png width="100%" alt="Dashboard Node-RED - Sem Movimento">

<img src=images/node-red-dashboard-status-motion-detected.png width="100%" alt="Dashboard Node-RED - Movimento Detectado">

<img src=images/node-red-dashboard-status-authorized.png width="100%" alt="Dashboard Node-RED - Acesso Autorizado">

<img src=images/node-red-dashboard-status-unauthorized.png width="100%" alt="Dashboard Node-RED - Acesso Não Autorizado">

* **Gravação Offline Confiável:** Mesmo sem rede Wi-Fi, todo o acesso continua sendo auditado no cartão SD.

<img src=images/log-saved-microsd.jpg width="100%" alt="Log Salvo no Cartão SD">

> 🎥 **Vídeo Demonstrativo do Funcionamento:** [Assistir no YouTube/Vídeo](https://youtube.com) *(Substitua pelo link real do seu vídeo)*

---

## 🚀 Como Executar o Projeto

1. **Hardware:** Monte os circuitos conforme o esquemático e verifique o GND comum entre as duas placas.
2. **Firmware Arduino:**
   * Abra a pasta do Arduino na Arduino IDE.
   * Instale as bibliotecas necessárias (`MFRC522`, `DS1302`).
   * Compile e envie o código para a placa Arduino Uno.
3. **Firmware BitDogLab (RP2040):**
   * Configure o ambiente com o **Pico SDK** e as fontes do **FreeRTOS**.
   * Configure suas credenciais Wi-Fi (SSID e Senha) e as configurações do Broker MQTT nos arquivos de configuração do projeto.
   * Compile o binário (`.uf2`) utilizando CMake/Make e transfira para o Raspberry Pi Pico W.
4. **Dashboard:**
   * Importe o fluxo JSON no **Node-RED** e configure o nó receptor MQTT para apontar para o mesmo broker.

---

## 👤 Autor

* **Michael Alves Ribeiro** - Residente EmbarcaTech