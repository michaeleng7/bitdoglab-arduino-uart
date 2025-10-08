void setup() {
  // Initialize serial communication at 9600 baud rate
  Serial.begin(9600);
}

void loop() {
  // Send a message to BitDogLab
  Serial.println("Ola BitDogLab!");
  
  // Wait for 2 seconds before sending the next message
  delay(2000); 
}