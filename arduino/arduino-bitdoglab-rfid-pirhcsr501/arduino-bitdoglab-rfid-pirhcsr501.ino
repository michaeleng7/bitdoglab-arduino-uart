/*******************************************************************************
 Access Control System (PIR Activated RFID)
 Implements a timeout to keep the RFID reader active after motion stops.
 Sends data to BitDogLab via Serial
*******************************************************************************/

// RFID Libraries
#include <MFRC522.h>
#include <SPI.h>

// --- Configuration Constants ---
// Timeout duration in milliseconds (e.g., 5000ms = 5 seconds)
const unsigned long RFID_ACTIVE_TIMEOUT = 5000; 

// --- RFID Configuration (SPI) ---
#define RST_PIN 9   // Reset Pin
#define SS_PIN 10   // Slave Select Pin
MFRC522 rfid(SS_PIN, RST_PIN);

String content; 

// --- PIR HC-SR501 Configuration ---
const int PIR_PIN = 2; // PIR sensor OUT pin connected to Digital Pin 2

// State variables
unsigned long lastMotionTime = 0; // Timestamp of the last time motion was detected
int currentPIRState = LOW; 
bool rfidActive = false; 

void setup() {
  Serial.begin(9600); // Initialize serial communication

  // --- Initialize SPI Communication ---
  SPI.begin();

  // --- PIR Initialization ---
  pinMode(PIR_PIN, INPUT); // Set the PIR sensor pin as input

  // Initially, RFID is assumed to be down
  
  Serial.println("Arduino: Access Control System Initialized.");
  Serial.println("RFID Reader is in low-power mode, waiting for motion.");
}

void loop() {
  
  // 1. PIR READING (MOTION CHECK)
  currentPIRState = digitalRead(PIR_PIN);

  // --- Logic for Motion Detection and Activation ---
  if (currentPIRState == HIGH) {
    
    // Update the time of the last motion detected (resets the timeout counter)
    lastMotionTime = millis(); 
    
    // a) Motion detected: Activate RFID reader if it's not already active
    if (!rfidActive) {
      rfid.PCD_Init(); // Power up and initialize the RFID reader
      rfidActive = true;
      Serial.println("PIR_STATUS:MOTION_DETECTED_RFID_ACTIVATED");
    }

    // Log continuous movement status (optional based on your requirement)
    // We only log if it's the start of a movement sequence for a cleaner log
    static int lastPIRLogState = LOW;
    if (currentPIRState != lastPIRLogState) {
       Serial.println("PIR_STATUS:MOTION_DETECTED"); 
    }
    lastPIRLogState = currentPIRState;
    
  } 

  // 2. RFID READING (Only possible when rfidActive is true)
  if (rfidActive && rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    
    // --- Logic for reading and converting the UID ---
    content = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) {
        content += "0";
      }
      content += String(rfid.uid.uidByte[i], HEX);
    }
    
    // Send the UID 
    Serial.print("RFID_UID:");
    Serial.println(content);

    // Halt the PICC 
    rfid.PICC_HaltA();
  }

  // 3. RFID SLEEP MANAGEMENT (TIMEOUT LOGIC)
  // Check if the RFID is active AND if the timeout has expired (time elapsed since last motion > timeout)
  if (rfidActive && (millis() - lastMotionTime > RFID_ACTIVE_TIMEOUT)) {
      
      rfid.PCD_SoftPowerDown(); // Put the RFID reader to sleep
      rfidActive = false;
      
      Serial.println("PIR_STATUS:NO_MOTION_RFID_SLEEP");
  }

  // Short delay to maintain responsiveness
  delay(100); 
}