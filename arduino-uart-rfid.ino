/*******************************************************************************
  RFID Reader - Envia UID para a BitDogLab via Serial
*******************************************************************************/
// Adicao de bibliotecas
#include <MFRC522.h>
#include <SPI.h>

// RFID reader pin definition
#define PINO_RST 9
#define PINO_SDA 10
MFRC522 rfid(PINO_SDA, PINO_RST);  // Creation of the "rfid" object with the SDA and RST pins defined

String content;  // Variable to store the UID of the tag read

void setup() {
  Serial.begin(9600); // Initializes serial communication with BitDogLab
  SPI.begin();        // Initializes SPI communication with the RFID reader
  rfid.PCD_Init();    // Initializes the RFID reader

  // Initial message on the serial to confirm the start of the program
  Serial.println("Arduino: Ready to read tags!");
}

void loop() {
  if (rfid.PICC_IsNewCardPresent()) {  // If there is a new tag

    if (rfid.PICC_ReadCardSerial()) {  // If the tag reading is successful

      // Converts the tag ID to a hexadecimal string
      content = ""; // Clears the string before each new read
      for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) {
          content += "0";
        }
        content += String(rfid.uid.uidByte[i], HEX);
      }

      // Sends the ID of the read tag to BitDogLab
      Serial.println(content);

      // Puts the reader on standby for a new reading
      rfid.PICC_HaltA();
    }
  }
}