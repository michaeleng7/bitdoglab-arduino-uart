/*******************************************************************************
 BitDogLab V7 - Access Control Hub (Arduino)
 Functions: 
 1. Reads PIR for motion detection and manages RFID reader power (Sleeping).
 2. Reads UID from tags and sends data to Pico W.
 3. Reads accurate Timestamp from DS1302 RTC.
 4. Allows manual time setting via Serial Monitor to ensure correct timezone.
 5. Ensures robust serial communication by assembling full log strings.
*******************************************************************************/

// --- LIBRARY INCLUSIONS ---
#include <MFRC522.h>
#include <SPI.h>
#include <Ds1302.h>     // Rafa Couto's Ds1302 library (3-wire)
#include <RTClib.h>     // Used for DateTime compile-time parsing
#include <Arduino.h>

// --- RTC DS1302 Configuration (3-Wire Protocol) ---
// Pins are (Ena, Clk, Dat) in the Ds1302 constructor.
const uint8_t RTC_ENA_PIN = 5; // Corresponds to RST/CE
const uint8_t RTC_CLK_PIN = 3; 
const uint8_t RTC_DAT_PIN = 4;

// RTC INSTANTIATION: Use Ds1302 class and (Ena, Clk, Dat) pin order.
Ds1302 rtc(RTC_ENA_PIN, RTC_CLK_PIN, RTC_DAT_PIN); 


// --- Configuration Constants ---
// Timeout duration in milliseconds to keep RFID reader active after last motion
const unsigned long RFID_ACTIVE_TIMEOUT = 5000; 
// ➡️ NOVO TEMPO DE ESPERA PARA AJUSTE MANUAL (30 segundos)
const unsigned long MANUAL_SET_TIMEOUT_MS = 30000; 

// --- RFID Configuration (SPI) ---
#define RST_PIN 9   // Reset Pin for MFRC522
#define SS_PIN 10   // Slave Select Pin for MFRC522
MFRC522 rfid(SS_PIN, RST_PIN);

String content; // Stores the RFID UID

// --- PIR HC-SR501 Configuration ---
const int PIR_PIN = 2; // PIR sensor OUT pin connected to Digital Pin 2

// State variables
unsigned long lastMotionTime = 0; 
int currentPIRState = LOW; 
bool rfidActive = false; 

// Tracks the last PIR state logged to avoid spamming "MOTION_DETECTED"
static int lastPIRLogState = LOW; 

// --- HELPER FUNCTION TO FORMAT THE TIMESTAMP ---
void formatTimestamp(char* buffer, size_t size) {
    Ds1302::DateTime dt;
    rtc.getDateTime(&dt);

    // Checks if the RTC time is valid (year 0-99).
    if (dt.year < 20 || dt.year > 99) { 
        snprintf(buffer, size, "RTC_UNSYNCED"); 
    } else {
        // Format: YYYY-MM-DD HH:MM:SS
        snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d",
                 dt.year + 2000, dt.month, dt.day,
                 dt.hour, dt.minute, dt.second);
    }
}

// --- HELPER FUNCTION TO SET RTC FROM SERIAL INPUT (MANUAL ADJUSTMENT) ---
/**
 * @brief Attempts to set the RTC time from a serial input string.
 * @details Format example: "2024 05 15 14 30 00" (Year Month Day Hour Minute Second)
 * @return True if time was set successfully, false otherwise.
 */
bool setRtcFromSerial() {
    Serial.println("RTC: To set time manually, type 'YYYY MM DD HH MM SS' and press Enter.");
    // ➡️ TEMPO DE ESPERA AUMENTADO
    Serial.print("RTC: Waiting for manual time input for ");
    Serial.print(MANUAL_SET_TIMEOUT_MS / 1000);
    Serial.println(" seconds...");

    unsigned long startTime = millis();
    String inputString = "";
    bool timeSet = false;

    // Usa a nova constante de timeout
    while (millis() - startTime < MANUAL_SET_TIMEOUT_MS && !timeSet) { 
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                if (inputString.length() > 0) {
                    Ds1302::DateTime dt;
                    int y, m, d, h, mn, s;
                    
                    // Parse the input string
                    if (sscanf(inputString.c_str(), "%d %d %d %d %d %d", &y, &m, &d, &h, &mn, &s) == 6) {
                        
                        // Populate the Ds1302 structure (Year is stored as 0-99)
                        dt.year   = y % 100; 
                        dt.month  = m;
                        dt.day    = d; // Day of the month
                        dt.hour   = h;
                        dt.minute = mn;
                        dt.second = s;
                        dt.dow    = 0; // Day of week calculation is complex; left at 0/default
                        
                        rtc.setDateTime(&dt);
                        rtc.start();
                        Serial.println("RTC: Time set manually successfully! RTC Running.");
                        timeSet = true;
                    } else {
                        Serial.println("RTC: Invalid time format. Please use 'YYYY MM DD HH MM SS'.");
                    }
                }
                inputString = ""; 
            } else {
                inputString += c;
            }
        }
    }
    
    if (!timeSet) {
        Serial.println("RTC: No manual time input received. Skipping manual set.");
    }
    return timeSet;
}

void setup() {
    Serial.begin(9600); // Initialize serial communication (to Pico W)

    // --- Initialize RTC (DS1302) ---
    rtc.init(); // Initialize the Ds1302 pins and communication

    // ➡️ Condição FORÇADA (temporariamente) para o ajuste manual
    if (true) { 
        Serial.println("RTC: Synchronization check. Attempting manual time adjustment.");
        
        if (!setRtcFromSerial()) { // Try manual set first
            
            // Fallback to compile time if manual set failed or timed out
            Serial.println("RTC: Manual time input skipped/failed. Falling back to compile time.");
            
            DateTime compileTime(F(__DATE__), F(__TIME__));

            Ds1302::DateTime dt;
            
            dt.year   = compileTime.year() % 100; 
            dt.month  = compileTime.month();
            dt.day    = compileTime.day();
            dt.hour   = compileTime.hour();
            dt.minute = compileTime.minute();
            dt.second = compileTime.second();
            
            // Set DOW to 0 (default) as complex conversion is avoided
            dt.dow    = 0; 
            
            rtc.setDateTime(&dt);
            rtc.start(); 
            Serial.println("RTC: Time set to compile date and started.");
        }
    } else {
        Serial.println("RTC: Time is set and oscillator is running.");
    }

    // --- Initialize SPI Communication (for MFRC522) ---
    SPI.begin();

    // --- PIR Initialization ---
    pinMode(PIR_PIN, INPUT); // Set the PIR sensor pin as input
    
    Serial.println("Arduino: Access Control System Initialized. RTC Operational.");
    Serial.println("RFID Reader is in low-power mode, waiting for motion.");
}

void loop() {
    char timestampBuffer[20]; // Buffer for the formatted timestamp
    char fullLogBuffer[100];  // Buffer to assemble the complete log line
    
    // 1. PIR READING (MOTION CHECK)
    currentPIRState = digitalRead(PIR_PIN);

    // --- Logic for Motion Detection and Activation ---
    if (currentPIRState == HIGH) {
        
        lastMotionTime = millis(); 
        
        // Get the current timestamp for logging any new motion event
        formatTimestamp(timestampBuffer, sizeof(timestampBuffer));
        
        // a) Motion detected: Activate RFID reader if it's not already active
        if (!rfidActive) {
            rfid.PCD_Init(); // Power up and initialize the RFID reader
            rfidActive = true;
            
            // Assemble and send the log (PIR ACTIVATED)
            snprintf(fullLogBuffer, sizeof(fullLogBuffer), 
                     "[%s] PIR_STATUS:MOTION_DETECTED_RFID_ACTIVATED", 
                     timestampBuffer);
            Serial.println(fullLogBuffer);
        }

        // b) Log continuous movement status (only log once when state changes from LOW to HIGH)
        if (currentPIRState != lastPIRLogState) {
            // Assemble and send the log (MOTION DETECTED)
            snprintf(fullLogBuffer, sizeof(fullLogBuffer), 
                     "[%s] PIR_STATUS:MOTION_DETECTED", 
                     timestampBuffer);
            Serial.println(fullLogBuffer);
        }
        lastPIRLogState = currentPIRState;
        
    } else {
        lastPIRLogState = LOW;
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
        
        // Get the timestamp for the access log
        formatTimestamp(timestampBuffer, sizeof(timestampBuffer));

        // Send the UID WITH TIMESTAMP (Format: [TS] RFID_UID:XXXX)
        Serial.print("[");
        Serial.print(timestampBuffer);
        Serial.print("] RFID_UID:");
        Serial.println(content);

        // Halt the PICC 
        rfid.PICC_HaltA();
    }

    // 3. RFID SLEEP MANAGEMENT (TIMEOUT LOGIC)
    if (rfidActive && (millis() - lastMotionTime > RFID_ACTIVE_TIMEOUT)) {
        
        formatTimestamp(timestampBuffer, sizeof(timestampBuffer));
        
        rfid.PCD_SoftPowerDown(); // Put the RFID reader to sleep
        rfidActive = false;
        
        // Assemble and send the log (RFID SLEEP)
        snprintf(fullLogBuffer, sizeof(fullLogBuffer), 
                 "[%s] PIR_STATUS:NO_MOTION_RFID_SLEEP", 
                 timestampBuffer);
        Serial.println(fullLogBuffer);
    }

    // --- OPTIMIZATION FOR SENSITIVITY ---
    if (!rfidActive) { 
        delay(100); 
    }
}