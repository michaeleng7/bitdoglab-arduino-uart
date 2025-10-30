/*******************************************************************************
 RFID Reader + BMP280 - Envia dados para a BitDogLab via Serial
*******************************************************************************/

// Bibliotecas do RFID (SPI)
#include <MFRC522.h>
#include <SPI.h>

// Bibliotecas do BMP280 (I2C)
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

// --- Configuração RFID (SPI) ---
#define PINO_RST 9
#define PINO_SDA 10
MFRC522 rfid(PINO_SDA, PINO_RST);

String content; // Variável para armazenar o UID lido

// --- Configuração BMP280 (I2C) ---
Adafruit_BMP280 bmp; // Cria o objeto BMP280 (usa o endereço I2C padrão)

void setup() {
  Serial.begin(9600); // Inicializa a comunicação serial

  // --- Inicialização RFID ---
  SPI.begin();       // Inicializa comunicação SPI
  rfid.PCD_Init();   // Inicializa o leitor RFID

  // --- Inicialização BMP280 ---
  if (!bmp.begin()) {
    Serial.println("Erro! BMP280 não encontrado. Verifique a fiação!");
    // Pode ser útil manter o loop preso aqui se o sensor for crucial:
    // while (1);
  }

  // Define modo de operação do BMP280 (opcional, mas recomendado)
  bmp.setSampling(
    Adafruit_BMP280::MODE_NORMAL,     // Modo Normal (medições contínuas)
    Adafruit_BMP280::SAMPLING_X2,     // Oversampling de Temperatura x2
    Adafruit_BMP280::SAMPLING_X16,    // Oversampling de Pressão x16
    Adafruit_BMP280::FILTER_X16,      // Filtro x16
    Adafruit_BMP280::STANDBY_MS_500   // Tempo de espera 500ms
  );

  Serial.println("Arduino: Pronto para ler tags e dados ambientais!");
}

void loop() {
  // 1. LEITURA E ENVIO DO RFID
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    content = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) {
        content += "0";
      }
      content += String(rfid.uid.uidByte[i], HEX);
    }
    
    // Envia o UID (com prefixo para identificação na BitDogLab)
    Serial.print("RFID_UID:");
    Serial.println(content);

    rfid.PICC_HaltA();
  }

  // 2. LEITURA E ENVIO DO BMP280
  // Fazemos a leitura a cada 2 segundos (ou um tempo adequado à sua aplicação)
  static unsigned long ultimaLeitura = 0;
  if (millis() - ultimaLeitura >= 2000) { // Envia dados a cada 2 segundos
    ultimaLeitura = millis();

    float temperatura = bmp.readTemperature(); // Temperatura em °C
    // A função readPressure() retorna em Pascals (Pa). 
    // Dividimos por 100 para obter Hectopascals (hPa) ou milibares (mbar).
    float pressao = bmp.readPressure() / 100.0; 

    // Envia a temperatura (com prefixo)
    Serial.print("TEMP_C:");
    Serial.println(temperatura, 2); // '2' para 2 casas decimais

    // Envia a pressão (com prefixo)
    Serial.print("PRESS_HPA:");
    Serial.println(pressao, 2); 
  }
}